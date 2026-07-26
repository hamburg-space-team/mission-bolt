use std::fs::File;
use std::io::{BufRead, BufReader, BufWriter, Read, Write};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use anyhow::{Context, Result};
use bolt_codec::{
    decode_payload, encode, normalize, Command, FrameEvent, MissionSpec, Sample, StreamDecoder,
};
use clap::Parser;
use serde::Serialize;

#[derive(Parser)]
#[command(name = "bolt-serial-bridge", version, about)]
struct Args {
    /// Enumerate available serial ports as JSON and exit (for the picker)
    #[arg(long)]
    list_ports: bool,

    /// Serial device (live mode), e.g. /dev/ttyUSB0 - or tcp://host:port for
    /// the debug station's UART-over-TCP bridge (baud is then the station's)
    #[arg(long)]
    port: Option<String>,

    /// Baud rate
    #[arg(long, default_value_t = 38400)]
    baud: u32,

    /// Replay a recorded .raw instead of opening a serial port
    #[arg(long)]
    replay: Option<String>,

    /// Tee every incoming byte to this .raw file
    #[arg(long)]
    raw: Option<String>,

    /// Mission spec id (pins layout + calibration).
    #[arg(long, default_value = "bolt")]
    mission: String,

    /// Stats emit interval in milliseconds
    #[arg(long, default_value_t = 500)]
    stats_ms: u64,

    /// Read the RXSM simulator's ASCII debug port (its D-SUB 9, NOT the RS-422
    /// telemetry link) and emit its status fields as JSON. Separate device and
    /// baud from the flight downlink - see the RXSM SIM user manual 3.5
    #[arg(long)]
    rxsm_debug: Option<String>,

    /// Baud for --rxsm-debug. The simulator's debug port is fixed at 115.2 kBd
    /// 8N1 (manual 3.5); this is not the 38.4 kBd flight link
    #[arg(long, default_value_t = 115_200)]
    rxsm_baud: u32,
}

/// One stdout message. Tagged so the extension can switch on `t`.
#[derive(Serialize)]
#[serde(tag = "t", rename_all = "snake_case")]
enum Out<'a> {
    Frame {
        ty: u8,
        name: &'a str,
        source: &'a str,
        seq: u8,
        tick: u16,
        timestamp_us: u32,
        crc_ok: bool,
        /// A validity mask says this data is not real (see Sample::suspect).
        suspect: bool,
        sample: Sample,
    },
    Resync {
        skipped: usize,
    },
    Stats {
        counts: &'a std::collections::BTreeMap<String, u64>,
        total: u64,
        crc_fail: u64,
        resync_bytes: u64,
        lo: bool,
        soe: bool,
        sods: bool,
        elapsed_ms: u128,
    },
    Log {
        level: &'a str,
        msg: String,
    },
    /// Latest RXSM simulator debug-port status. Ground equipment, not flight
    /// telemetry; an open map because the simulator's label set is open-ended
    Rxsm {
        fields: &'a std::collections::BTreeMap<String, String>,
    },
}

fn emit(msg: &Out) {
    // One JSON object per line. Locking stdout keeps it interleave-safe
    // against the command thread's logs.
    let stdout = std::io::stdout();
    let mut lock = stdout.lock();
    if serde_json::to_writer(&mut lock, msg).is_ok() {
        let _ = lock.write_all(b"\n");
        let _ = lock.flush();
    }
}

fn log(level: &str, msg: impl Into<String>) {
    emit(&Out::Log {
        level,
        msg: msg.into(),
    });
}

/// One discovered serial port, for the extension's picker.
#[derive(Serialize)]
struct PortInfo {
    port: String,
    kind: &'static str,
    manufacturer: Option<String>,
    product: Option<String>,
    vid: Option<u16>,
    pid: Option<u16>,
    serial_number: Option<String>,
}

/// Print `{"t":"ports","ports":[...]}` (USB adapters first, richest info).
fn list_ports() {
    let ports = enumerate_ports();
    let obj = serde_json::json!({ "t": "ports", "ports": ports });
    println!("{obj}");
}

#[cfg(feature = "serial")]
fn enumerate_ports() -> Vec<PortInfo> {
    use serialport::SerialPortType;
    let mut ports: Vec<PortInfo> = serialport::available_ports()
        .unwrap_or_default()
        .into_iter()
        .map(|p| match p.port_type {
            SerialPortType::UsbPort(u) => PortInfo {
                port: p.port_name,
                kind: "usb",
                manufacturer: u.manufacturer,
                product: u.product,
                vid: Some(u.vid),
                pid: Some(u.pid),
                serial_number: u.serial_number,
            },
            SerialPortType::PciPort => plain(p.port_name, "pci"),
            SerialPortType::BluetoothPort => plain(p.port_name, "bluetooth"),
            SerialPortType::Unknown => plain(p.port_name, "unknown"),
        })
        .collect();
    // USB adapters (the real RXSM link) first, then the rest.
    ports.sort_by_key(|p| (p.kind != "usb", p.port.clone()));
    ports
}

#[cfg(not(feature = "serial"))]
fn enumerate_ports() -> Vec<PortInfo> {
    Vec::new()
}

#[cfg(feature = "serial")]
fn plain(port: String, kind: &'static str) -> PortInfo {
    PortInfo {
        port,
        kind,
        manufacturer: None,
        product: None,
        vid: None,
        pid: None,
        serial_number: None,
    }
}

// Pull every "Label: Value" out of one debug-port line. The simulator lays
// its status out in columns (manual 3.5):
//
//     Error Inhibit: OFF        Byte Dropout Rate: 2^-11
//       |_ Duration: 800ms         Bit Error Rate: 2^-18
//
// Columns are runs of >=2 spaces, labels may contain single spaces - split on
// the runs. Unknown labels pass through (the manual's listing is open-ended)
fn parse_debug_line(line: &str, out: &mut std::collections::BTreeMap<String, String>) -> bool {
    if line.trim_start().starts_with('#') {
        return false;
    }
    let bytes = line.as_bytes();
    let gap_at = |i: usize| bytes[i] == b' ' && bytes.get(i + 1) == Some(&b' ');

    let mut changed = false;
    let mut pos = 0usize; // start of the next label
    while let Some(rel) = line[pos..].find(':') {
        let colon = pos + rel;
        let label = line[pos..colon]
            .trim()
            // continuation rows are prefixed with an arrow glyph
            .trim_start_matches(['\u{21b3}', '>', '-', '|', '_'])
            .trim();

        // skip the alignment padding FIRST - it is itself >=2 spaces, so a
        // gap-split without skipping severs every value from its label
        let mut vs = colon + 1;
        while vs < bytes.len() && bytes[vs] == b' ' {
            vs += 1;
        }
        let mut ve = vs;
        while ve < bytes.len() && !gap_at(ve) {
            ve += 1; // only ever stops on an ASCII space or EOL -> char boundary
        }
        let value = line[vs..ve].trim();

        if !label.is_empty()
            && !value.is_empty()
            && out.get(label).map(String::as_str) != Some(value)
        {
            out.insert(label.to_string(), value.to_string());
            changed = true;
        }

        pos = ve;
        if pos >= line.len() {
            break;
        }
    }
    changed
}

// Read the simulator's ASCII debug port and republish its status as JSON.
// Deliberately NOT BufReader::read_line: TimeoutRead maps a timeout to Ok(0),
// which read_line treats as EOF - assemble lines ourselves and treat Ok(0)
// as "nothing yet"
fn run_rxsm_debug(port: &str, baud: u32) -> Result<()> {
    log(
        "info",
        format!("rxsm debug port {port} @ {baud} 8N1 (ground equipment, not flight data)"),
    );
    let (mut source, _) = open_serial(port, baud)?;

    let mut fields = std::collections::BTreeMap::new();
    let mut acc = String::new();
    let mut buf = [0u8; 512];
    let mut seen_any = false;
    // opening the port lands mid-line; the first fragment would parse into a
    // bogus field that sticks forever - sync to the first newline
    let mut synced = false;

    loop {
        let n = match source.read(&mut buf) {
            Ok(0) => continue, // read timeout (the port blocked first), not EOF
            Ok(n) => n,
            Err(e) if e.kind() == std::io::ErrorKind::TimedOut => continue,
            Err(e) => return Err(e).context("read rxsm debug port"),
        };
        if !seen_any {
            seen_any = true;
            log("info", "rxsm debug port: first bytes received");
        }
        // Lossy: the manual's own sample contains a non-ASCII arrow, and line
        // noise must not kill the reader
        acc.push_str(&String::from_utf8_lossy(&buf[..n]));

        while let Some(idx) = acc.find('\n') {
            let line: String = acc.drain(..=idx).collect();
            if !synced {
                synced = true; // partial first line - we opened mid-transmission
                continue;
            }
            let line = line.trim_end_matches(['\r', '\n']);
            if line.trim().is_empty() {
                continue;
            }
            // Echo the raw line: the label set is open, so this is the only way
            // to see a field the parser does not understand yet
            log("info", format!("rxsm < {line}"));
            if parse_debug_line(line, &mut fields) {
                emit(&Out::Rxsm { fields: &fields });
            }
        }
        // A port that never sends a newline must not grow the buffer forever
        if acc.len() > 8192 {
            log(
                "warn",
                "rxsm debug port: 8 KB without a newline - discarding",
            );
            acc.clear();
        }
    }
}

fn main() -> Result<()> {
    let args = Args::parse();

    if args.list_ports {
        list_ports();
        return Ok(());
    }

    if let Some(port) = &args.rxsm_debug {
        return run_rxsm_debug(port, args.rxsm_baud);
    }

    let spec: &'static MissionSpec =
        bolt_codec::find_mission(&args.mission).unwrap_or_else(bolt_codec::default_mission);
    log(
        "info",
        format!("mission = {} ({})", spec.id, spec.display_name),
    );

    let mut tee = match &args.raw {
        Some(path) => {
            log("info", format!("recording raw -> {path}"));
            Some(BufWriter::new(
                File::create(path).with_context(|| format!("create {path}"))?,
            ))
        }
        None => None,
    };

    // For a live serial port a read timeout surfaces as 0 bytes and must
    // NOT end the loop; for TCP, replay and stdin, 0 bytes is real EOF
    // (a closed station connection must end the bridge, not spin it).
    let is_tcp = args
        .port
        .as_deref()
        .is_some_and(|p| p.starts_with("tcp://"));
    let is_serial = args.replay.is_none() && args.port.is_some() && !is_tcp;
    let break_on_eof = !is_serial;

    // Open the byte source.
    let (mut source, port_handle): (Box<dyn Read + Send>, Option<SharedPort>) =
        match (&args.replay, &args.port) {
            (Some(path), _) => {
                log("info", format!("replaying {path}"));
                (
                    Box::new(File::open(path).with_context(|| format!("open {path}"))?),
                    None,
                )
            }
            (None, Some(port)) => open_source(port, args.baud)?,
            (None, None) => {
                log("info", "no --port/--replay: reading raw stream from stdin");
                (Box::new(std::io::stdin()), None)
            }
        };

    // Uplink: read command JSON lines from stdin and write encoded frames
    // to the serial port. Only meaningful in live mode.
    let running = Arc::new(AtomicBool::new(true));
    if let Some(port) = port_handle.clone() {
        spawn_command_reader(port, running.clone());
    }

    let mut dec = StreamDecoder::new();
    let mut counts: std::collections::BTreeMap<String, u64> = std::collections::BTreeMap::new();
    let mut total = 0u64;
    let mut crc_fail = 0u64;
    let mut resync_bytes = 0u64;
    let mut lo = false;
    let mut soe = false;
    let mut sods = false;
    let start = Instant::now();
    let mut last_stats = Instant::now();
    let stats_iv = Duration::from_millis(args.stats_ms);

    let mut buf = [0u8; 4096];
    loop {
        // Graceful stop: the extension sends {"cmd":"stop"} on Live->Idle so we
        // break here and hit the clean tee flush below, instead of being hard
        // -killed mid-buffer (which would drop the tail of the recording).
        if !running.load(Ordering::SeqCst) {
            break;
        }
        let n = source.read(&mut buf)?;
        if n == 0 {
            if break_on_eof {
                break; // replay done
            }
            // Serial idle (read timeout, ~200 ms): flush the raw so at most one
            // timeout's worth is ever unwritten even on a hard kill, then emit
            // periodic stats.
            if let Some(w) = tee.as_mut() {
                w.flush()?;
            }
            if last_stats.elapsed() >= stats_iv {
                emit(&Out::Stats {
                    counts: &counts,
                    total,
                    crc_fail,
                    resync_bytes,
                    lo,
                    soe,
                    sods,
                    elapsed_ms: start.elapsed().as_millis(),
                });
                last_stats = Instant::now();
            }
            continue;
        }
        if let Some(w) = tee.as_mut() {
            w.write_all(&buf[..n])?;
        }
        dec.push(&buf[..n]);

        while let Some(ev) = dec.pull() {
            match ev {
                FrameEvent::Resync { skipped } => {
                    resync_bytes += skipped as u64;
                    emit(&Out::Resync { skipped });
                }
                FrameEvent::Frame(f) => {
                    total += 1;
                    if !f.crc_ok {
                        crc_fail += 1;
                    }
                    match decode_payload(f.header.ty, &f.payload) {
                        Ok(payload) => {
                            let sample = normalize(&payload, spec);
                            if let Sample::Status {
                                signal: Some(sig), ..
                            } = &sample
                            {
                                lo = sig.lo;
                                soe = sig.soe;
                                sods = sig.sods;
                            }
                            let pt = bolt_codec::PayloadType::from_u8(f.header.ty);
                            let name = pt.map_or("unknown", bolt_codec::PayloadType::name);
                            // frame_source, not p.source(): the generic types
                            // carry their real origin in the payload
                            let src = bolt_codec::frame_source(f.header.ty, &sample);
                            *counts.entry(name.to_string()).or_default() += 1;
                            emit(&Out::Frame {
                                ty: f.header.ty,
                                name,
                                source: src,
                                seq: f.header.sequence,
                                tick: f.header.tick,
                                timestamp_us: f.header.timestamp_us,
                                crc_ok: f.crc_ok,
                                suspect: sample.suspect(),
                                sample,
                            });
                        }
                        Err(e) => log("warn", format!("decode: {e}")),
                    }
                }
            }
        }

        if last_stats.elapsed() >= stats_iv {
            emit(&Out::Stats {
                counts: &counts,
                total,
                crc_fail,
                resync_bytes,
                lo,
                soe,
                sods,
                elapsed_ms: start.elapsed().as_millis(),
            });
            last_stats = Instant::now();
        }
    }

    running.store(false, Ordering::SeqCst);
    if let Some(w) = tee.as_mut() {
        w.flush()?;
    }
    emit(&Out::Stats {
        counts: &counts,
        total,
        crc_fail,
        resync_bytes,
        lo,
        soe,
        sods,
        elapsed_ms: start.elapsed().as_millis(),
    });
    log("info", "stream ended");
    Ok(())
}

// --- serial + uplink -------------------------------------------------------

// Uplink writer half of the byte source; serial handle or TCP stream
type SharedPort = Arc<Mutex<Box<dyn Write + Send>>>;

/// tcp://host:port or a local serial device
fn open_source(spec: &str, baud: u32) -> Result<(Box<dyn Read + Send>, Option<SharedPort>)> {
    match spec.strip_prefix("tcp://") {
        Some(addr) => open_tcp(addr),
        None => open_serial(spec, baud),
    }
}

// The debug station serves the RXSM UART as a raw TCP socket. Blocking
// reads: 0 bytes means the station closed, which main treats as EOF
fn open_tcp(addr: &str) -> Result<(Box<dyn Read + Send>, Option<SharedPort>)> {
    let stream =
        std::net::TcpStream::connect(addr).with_context(|| format!("connect tcp {addr}"))?;
    let _ = stream.set_nodelay(true); // uplink frames are 14 B, don't batch them
    log("info", format!("tcp {addr} connected"));
    let writer = stream.try_clone().context("clone tcp for writing")?;
    let shared: SharedPort = Arc::new(Mutex::new(Box::new(writer)));
    Ok((Box::new(stream), Some(shared)))
}

#[cfg(feature = "serial")]
fn open_serial(port: &str, baud: u32) -> Result<(Box<dyn Read + Send>, Option<SharedPort>)> {
    let handle = serialport::new(port, baud)
        .timeout(Duration::from_millis(200))
        .open()
        .with_context(|| format!("open serial {port} @ {baud}"))?;
    log("info", format!("serial {port} @ {baud} open"));
    let reader = handle.try_clone().context("clone serial for reading")?;
    let shared: SharedPort = Arc::new(Mutex::new(Box::new(handle)));
    Ok((Box::new(TimeoutRead(reader)), Some(shared)))
}

#[cfg(not(feature = "serial"))]
fn open_serial(_port: &str, _baud: u32) -> Result<(Box<dyn Read + Send>, Option<SharedPort>)> {
    anyhow::bail!("built without the `serial` feature; use tcp://host:port, --replay or stdin")
}

/// Serial reads return WouldBlock/TimedOut on idle; map those to 0-length
/// reads so the main loop keeps polling instead of erroring out.
#[cfg(feature = "serial")]
struct TimeoutRead(Box<dyn serialport::SerialPort>);

#[cfg(feature = "serial")]
impl Read for TimeoutRead {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        match self.0.read(buf) {
            Ok(n) => Ok(n),
            Err(e) if e.kind() == std::io::ErrorKind::TimedOut => Ok(0),
            Err(e) => Err(e),
        }
    }
}

#[derive(serde::Deserialize)]
struct CmdMsg {
    cmd: String,
    #[serde(default)]
    opcode: Option<u8>,
    #[serde(default)]
    payload: Vec<u8>,
}

fn spawn_command_reader(port: SharedPort, running: Arc<AtomicBool>) {
    std::thread::spawn(move || {
        let stdin = std::io::stdin();
        let mut seq = 0u8;
        for line in BufReader::new(stdin).lines() {
            if !running.load(Ordering::SeqCst) {
                break;
            }
            let Ok(line) = line else { break };
            if line.trim().is_empty() {
                continue;
            }
            match serde_json::from_str::<CmdMsg>(&line) {
                // Sentinel from the extension's disconnect(): stop the stream so
                // the main loop breaks and flushes the raw tee cleanly.
                Ok(msg) if msg.cmd == "stop" => {
                    running.store(false, Ordering::SeqCst);
                    break;
                }
                Ok(msg) => send_command(&port, &mut seq, msg),
                Err(e) => log("warn", format!("bad command json: {e}")),
            }
        }
    });
}

fn send_command(port: &SharedPort, seq: &mut u8, msg: CmdMsg) {
    let cmd = match msg.cmd.as_str() {
        "raw" => match msg.opcode {
            Some(op) => {
                let mut p = heapless_vec(&msg.payload);
                Command::Raw {
                    opcode: op,
                    payload: std::mem::take(&mut p),
                }
            }
            None => {
                log("warn", "raw command needs an opcode");
                return;
            }
        },
        name => match Command::from_name(name) {
            Some(c) => c,
            None => {
                log("warn", format!("unknown command '{name}'"));
                return;
            }
        },
    };
    let this_seq = *seq;
    let bytes = encode(&cmd, seq);
    match port.lock().unwrap().write_all(&bytes) {
        Ok(()) => log(
            "info",
            format!("uplink '{}' sent (seq {this_seq})", msg.cmd),
        ),
        Err(e) => log("error", format!("uplink write failed: {e}")),
    }
}

fn heapless_vec(bytes: &[u8]) -> heapless::Vec<u8, { bolt_codec::MAX_PAYLOAD }> {
    let mut v = heapless::Vec::new();
    let _ = v.extend_from_slice(&bytes[..bytes.len().min(bolt_codec::MAX_PAYLOAD)]);
    v
}

#[cfg(test)]
mod tests {
    use super::*;

    // Verbatim from the RXSM SIM user manual, listing 3.1: two columns per
    // line, a continuation arrow, and a trailing comment line
    const MANUAL_SAMPLE: &str = concat!(
        "Error Inhibit: OFF        Byte Dropout Rate: 2^-11\r\n",
        "  \u{21b3} Duration: 800ms          Bit Error Rate: 2^-18\r\n",
        "# many more lines\r\n",
    );

    fn parse_all(s: &str) -> std::collections::BTreeMap<String, String> {
        let mut fields = std::collections::BTreeMap::new();
        for line in s.lines() {
            parse_debug_line(line.trim_end_matches(['\r', '\n']), &mut fields);
        }
        fields
    }

    // Verbatim from the real simulator (the scanned manual was misleading:
    // values are column-padded and the label is "Dropout Duration")
    const REAL: &str = "Error Inhibit: ON         Byte Dropout Rate:   0          Dropout Duration:  7 B         Bit Error Rate:   0  ";

    #[test]
    fn parses_the_real_simulator_line() {
        let f = parse_all(REAL);
        assert_eq!(f.get("Error Inhibit").map(String::as_str), Some("ON"));
        assert_eq!(f.get("Byte Dropout Rate").map(String::as_str), Some("0"));
        assert_eq!(f.get("Dropout Duration").map(String::as_str), Some("7 B"));
        assert_eq!(f.get("Bit Error Rate").map(String::as_str), Some("0"));
        assert_eq!(f.len(), 4, "got {f:?}");
    }

    #[test]
    fn parses_every_column_of_the_manual_sample() {
        let f = parse_all(MANUAL_SAMPLE);
        assert_eq!(f.get("Error Inhibit").map(String::as_str), Some("OFF"));
        assert_eq!(
            f.get("Byte Dropout Rate").map(String::as_str),
            Some("2^-11")
        );
        assert_eq!(f.get("Duration").map(String::as_str), Some("800ms"));
        assert_eq!(f.get("Bit Error Rate").map(String::as_str), Some("2^-18"));
        assert_eq!(f.len(), 4, "the comment line must not become a field");
    }

    #[test]
    fn labels_keep_their_internal_spaces() {
        // Splitting on single spaces would shred "Byte Dropout Rate"
        let f = parse_all("Byte Dropout Rate: 2^-11\r\n");
        assert_eq!(f.keys().collect::<Vec<_>>(), vec!["Byte Dropout Rate"]);
    }

    #[test]
    fn unknown_labels_pass_through() {
        // The manual documents only a few fields ("# many more lines"), so the
        // parser must not filter on a known set
        let f = parse_all("Some Future Knob: 42\r\n");
        assert_eq!(f.get("Some Future Knob").map(String::as_str), Some("42"));
    }

    #[test]
    fn only_real_changes_are_reported() {
        let mut fields = std::collections::BTreeMap::new();
        assert!(
            parse_debug_line("Error Inhibit: OFF", &mut fields),
            "first value is a change"
        );
        assert!(
            !parse_debug_line("Error Inhibit: OFF", &mut fields),
            "same value must not re-emit"
        );
        assert!(
            parse_debug_line("Error Inhibit: ON", &mut fields),
            "flip must re-emit"
        );
    }

    // A serial read returns whatever bytes happen to be there; a status line
    // routinely arrives in pieces, split across 200 ms timeout windows. The
    // reader must stitch them back together instead of parsing fragments
    #[test]
    fn lines_split_across_reads_are_reassembled() {
        let chunks = ["Error Inh", "ibit: OFF        Byte Dropout Rate: 2^-11\r\n"];
        let mut fields = std::collections::BTreeMap::new();
        let mut acc = String::new();
        for c in chunks {
            acc.push_str(c);
            while let Some(idx) = acc.find('\n') {
                let line: String = acc.drain(..=idx).collect();
                parse_debug_line(line.trim_end_matches(['\r', '\n']), &mut fields);
            }
        }
        assert_eq!(fields.get("Error Inhibit").map(String::as_str), Some("OFF"));
        assert_eq!(
            fields.get("Byte Dropout Rate").map(String::as_str),
            Some("2^-11")
        );
        assert_eq!(
            fields.len(),
            2,
            "a fragment must never become its own field: {fields:?}"
        );
    }

    #[test]
    fn junk_lines_are_ignored() {
        let f = parse_all("garbage without a colon\r\n\r\n# comment: not a field\r\n");
        assert!(f.is_empty(), "got {f:?}");
    }
}
