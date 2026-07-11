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

    /// Serial device (live mode), e.g. /dev/ttyUSB0
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
    emit(&Out::Log { level, msg: msg.into() });
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
    PortInfo { port, kind, manufacturer: None, product: None, vid: None, pid: None, serial_number: None }
}

fn main() -> Result<()> {
    let args = Args::parse();

    if args.list_ports {
        list_ports();
        return Ok(());
    }

    let spec: &'static MissionSpec =
        bolt_codec::find_mission(&args.mission).unwrap_or_else(bolt_codec::default_mission);
    log("info", format!("mission = {} ({})", spec.id, spec.display_name));

    let mut tee = match &args.raw {
        Some(path) => {
            log("info", format!("recording raw -> {path}"));
            Some(BufWriter::new(File::create(path).with_context(|| format!("create {path}"))?))
        }
        None => None,
    };

    // For a live serial port a read timeout surfaces as 0 bytes and must
    // NOT end the loop; for a file/stdin replay, 0 bytes is real EOF.
    let is_serial = args.replay.is_none() && args.port.is_some();
    let break_on_eof = !is_serial;

    // Open the byte source.
    let (mut source, port_handle): (Box<dyn Read + Send>, Option<SharedPort>) = match (&args.replay,
        &args.port)
    {
        (Some(path), _) => {
            log("info", format!("replaying {path}"));
            (Box::new(File::open(path).with_context(|| format!("open {path}"))?), None)
        }
        (None, Some(port)) => open_serial(port, args.baud)?,
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
                            if let Sample::Status { signal: Some(sig), .. } = &sample {
                                lo = sig.lo;
                                soe = sig.soe;
                                sods = sig.sods;
                            }
                            let pt = bolt_codec::PayloadType::from_u8(f.header.ty);
                            let (name, src) =
                                pt.map_or(("unknown", "system"), |p| (p.name(), p.source()));
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

#[cfg(feature = "serial")]
type SharedPort = Arc<Mutex<Box<dyn serialport::SerialPort>>>;
#[cfg(not(feature = "serial"))]
type SharedPort = Arc<Mutex<()>>;

#[cfg(feature = "serial")]
fn open_serial(port: &str, baud: u32) -> Result<(Box<dyn Read + Send>, Option<SharedPort>)> {
    let handle = serialport::new(port, baud)
        .timeout(Duration::from_millis(200))
        .open()
        .with_context(|| format!("open serial {port} @ {baud}"))?;
    log("info", format!("serial {port} @ {baud} open"));
    let reader = handle.try_clone().context("clone serial for reading")?;
    let shared: SharedPort = Arc::new(Mutex::new(handle));
    Ok((Box::new(TimeoutRead(reader)), Some(shared)))
}

#[cfg(not(feature = "serial"))]
fn open_serial(_port: &str, _baud: u32) -> Result<(Box<dyn Read + Send>, Option<SharedPort>)> {
    anyhow::bail!("built without the `serial` feature; use --replay or pipe raw on stdin")
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

#[cfg(feature = "serial")]
fn send_command(port: &SharedPort, seq: &mut u8, msg: CmdMsg) {
    let cmd = match msg.cmd.as_str() {
        "raw" => match msg.opcode {
            Some(op) => {
                let mut p = heapless_vec(&msg.payload);
                Command::Raw { opcode: op, payload: std::mem::take(&mut p) }
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
        Ok(()) => log("info", format!("uplink '{}' sent (seq {this_seq})", msg.cmd)),
        Err(e) => log("error", format!("uplink write failed: {e}")),
    }
}

#[cfg(not(feature = "serial"))]
fn send_command(_port: &SharedPort, _seq: &mut u8, _msg: CmdMsg) {}

#[cfg(feature = "serial")]
fn heapless_vec(bytes: &[u8]) -> heapless::Vec<u8, { bolt_codec::MAX_PAYLOAD }> {
    let mut v = heapless::Vec::new();
    let _ = v.extend_from_slice(&bytes[..bytes.len().min(bolt_codec::MAX_PAYLOAD)]);
    v
}
