//! bolt-station - the debug station's daemon, replacing ser2net.
//!
//! Owns the downlink UART (the ST-Link V3 virtual COM port) and re-serves
//! the raw stream to any number of clients, relaying their uplink back
//! out. The same bytes feed the HTTP API and the kiosk dashboard.

mod http;
mod probe;
mod state;
mod store;
mod tracker;

use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use anyhow::{Context, Result};
use bolt_codec::wire::{FrameEvent, StreamDecoder};
use bolt_codec::{decode_payload, default_mission, normalize, Sample};
use clap::Parser;

use state::Live;
use store::Store;
use tracker::Tracker;

#[derive(Parser, Clone)]
#[command(name = "bolt-station", version, about)]
pub struct Config {
    /// Downlink byte source: serial device or tcp://host:port (dev mode).
    /// Default is the ST-Link V3 virtual COM port (udev: 99-bolt-station)
    #[arg(long, default_value = "/dev/ttySTLINK")]
    port: String,

    /// Baud rate of the serial source. 230400 over the ST-Link VCP (just
    /// under the TTL/CAN part's ~250 kBd); the RXSM link is 38400
    #[arg(long, default_value_t = 230_400)]
    pub baud: u32,

    /// The rate the FLIGHT link runs at, for the load figure. The bench
    /// link being faster must not hide how full the real downlink is
    #[arg(long, default_value_t = 38400)]
    pub flight_baud: u32,

    /// Raw re-serve listener (the ser2net replacement, multi-client)
    #[arg(long, default_value = "0.0.0.0:5000")]
    raw_listen: String,

    /// HTTP API listener
    #[arg(long, default_value = "0.0.0.0:8080")]
    http: String,

    /// SQLite database for test runs
    #[arg(long, default_value = "/var/lib/bolt-station/station.db")]
    db: String,

    /// Chip-UID -> board map ("<uid> <board>" per line)
    #[arg(long, default_value = "/etc/bolt-station/uids.conf")]
    pub uid_map: String,

    /// OpenOCD TCL RPC address for debugger-side reads
    #[arg(long, default_value = "127.0.0.1:6601")]
    pub openocd_tcl: String,

    /// .noinit base address (hex), the fallback when uids.conf carries none
    #[arg(long, value_parser = parse_hex)]
    pub noinit_addr: Option<u32>,

    /// Kiosk dashboard root; served at / by the same port as the API
    #[arg(long, default_value = "/usr/local/share/bolt-station/ui")]
    pub ui_dir: String,
}

fn parse_hex(s: &str) -> std::result::Result<u32, String> {
    u32::from_str_radix(s.trim_start_matches("0x"), 16).map_err(|e| e.to_string())
}

fn now_utc() -> String {
    chrono::Utc::now().to_rfc3339_opts(chrono::SecondsFormat::Millis, true)
}

/// Clients of the raw re-serve port; dead ones are dropped on write failure.
type Clients = Arc<Mutex<Vec<TcpStream>>>;
/// Writer back to the byte source (uplink path).
pub type Uplink = Arc<Mutex<Option<Box<dyn Write + Send>>>>;
/// An open byte source: reader plus (for live sources) the uplink writer.
type Source = (Box<dyn Read + Send>, Option<Box<dyn Write + Send>>);

fn main() -> Result<()> {
    let cfg = Config::parse();

    let live = Arc::new(Mutex::new(Live::new(Instant::now())));
    let store = Arc::new(Mutex::new(Store::open(&cfg.db)?));
    let tracker = Arc::new(Mutex::new(Tracker::new()));
    let clients: Clients = Arc::new(Mutex::new(Vec::new()));
    let uplink: Uplink = Arc::new(Mutex::new(None));

    raw_server(&cfg.raw_listen, clients.clone(), uplink.clone())?;

    {
        let api = http::Api {
            cfg: cfg.clone(),
            live: live.clone(),
            store: store.clone(),
            tracker: tracker.clone(),
            uplink: uplink.clone(),
            cmd_seq: Arc::new(Mutex::new(0u8)),
            noinit_at: Arc::new(Mutex::new(None)),
        };
        let listen = cfg.http.clone();
        std::thread::spawn(move || {
            if let Err(e) = http::serve(&api, &listen) {
                eprintln!("station: http server died: {e}");
                std::process::exit(1);
            }
        });
    }

    // never give up on the source: probes move, adapters get replugged
    loop {
        match open_source(&cfg) {
            Ok((mut src, writer)) => {
                *uplink.lock().unwrap() = writer;
                eprintln!("station: source {} open", cfg.port);
                if let Err(e) = pump(&mut *src, &live, &store, &tracker, &clients) {
                    eprintln!("station: source lost ({e}) - retrying");
                }
                *uplink.lock().unwrap() = None;
            }
            Err(e) => eprintln!("station: cannot open {} ({e}) - retrying", cfg.port),
        }
        std::thread::sleep(Duration::from_secs(2));
    }
}

/// Accept raw-stream clients; bytes from one are uplink for the serial port.
fn raw_server(listen: &str, clients: Clients, uplink: Uplink) -> Result<()> {
    let listener = TcpListener::bind(listen).with_context(|| format!("raw listen {listen}"))?;
    eprintln!("station: raw stream on {listen}");
    std::thread::spawn(move || {
        for stream in listener.incoming().flatten() {
            let _ = stream.set_nodelay(true);
            if let Ok(reader) = stream.try_clone() {
                let up = uplink.clone();
                std::thread::spawn(move || client_uplink(reader, &up));
            }
            clients.lock().unwrap().push(stream);
        }
    });
    Ok(())
}

fn client_uplink(mut reader: TcpStream, uplink: &Uplink) {
    let mut buf = [0u8; 256];
    loop {
        match reader.read(&mut buf) {
            Ok(0) | Err(_) => return, // client gone; the write side gets pruned on next broadcast
            Ok(n) => {
                if let Some(w) = uplink.lock().unwrap().as_mut() {
                    let _ = w.write_all(&buf[..n]).and_then(|()| w.flush());
                }
            }
        }
    }
}

fn open_source(cfg: &Config) -> Result<Source> {
    if let Some(addr) = cfg.port.strip_prefix("tcp://") {
        let s = TcpStream::connect(addr).with_context(|| format!("connect {addr}"))?;
        let _ = s.set_nodelay(true);
        s.set_read_timeout(Some(Duration::from_millis(200)))?;
        let w = s.try_clone()?;
        return Ok((Box::new(s), Some(Box::new(w))));
    }
    open_serial(cfg)
}

#[cfg(feature = "serial")]
fn open_serial(cfg: &Config) -> Result<Source> {
    let port = serialport::new(&cfg.port, cfg.baud)
        .timeout(Duration::from_millis(200))
        .open()
        .with_context(|| format!("open {}", cfg.port))?;
    let writer = port.try_clone().context("clone serial for writing")?;
    Ok((Box::new(port), Some(Box::new(writer))))
}

#[cfg(not(feature = "serial"))]
fn open_serial(_cfg: &Config) -> Result<Source> {
    anyhow::bail!("built without the serial feature - use tcp://")
}

fn pump(
    src: &mut dyn Read,
    live: &Arc<Mutex<Live>>,
    store: &Arc<Mutex<Store>>,
    tracker: &Arc<Mutex<Tracker>>,
    clients: &Clients,
) -> Result<()> {
    let spec = default_mission();
    let mut dec = StreamDecoder::new();
    let mut buf = [0u8; 4096];
    loop {
        let n = match src.read(&mut buf) {
            Ok(n) => n,
            // silence, not a lost source (serial: TimedOut, tcp: WouldBlock)
            Err(e)
                if matches!(
                    e.kind(),
                    std::io::ErrorKind::TimedOut | std::io::ErrorKind::WouldBlock
                ) =>
            {
                let now = Instant::now();
                tracker
                    .lock()
                    .unwrap()
                    .on_idle_tick(&store.lock().unwrap(), &now_utc(), now)?;
                continue;
            }
            Err(e) => return Err(e.into()),
        };
        if n == 0 {
            anyhow::bail!("EOF");
        }
        {
            let mut l = live.lock().unwrap();
            l.total_bytes += n as u64;
            l.window.record_bytes(Instant::now(), n as u64);
        }
        broadcast(clients, &buf[..n]);
        dec.push(&buf[..n]);
        while let Some(ev) = dec.pull() {
            if let FrameEvent::Frame(f) = ev {
                on_frame(&f, spec, live, store, tracker)?;
            }
        }
    }
}

fn broadcast(clients: &Clients, bytes: &[u8]) {
    let mut list = clients.lock().unwrap();
    list.retain_mut(|c| c.write_all(bytes).is_ok());
}

fn on_frame(
    f: &bolt_codec::wire::Frame,
    spec: &'static bolt_codec::MissionSpec,
    live: &Arc<Mutex<Live>>,
    store: &Arc<Mutex<Store>>,
    tracker: &Arc<Mutex<Tracker>>,
) -> Result<()> {
    let now = Instant::now();
    {
        let mut l = live.lock().unwrap();
        l.total_frames += 1;
        if !f.crc_ok {
            l.total_crc_fail += 1;
        }
        l.window.record(now, f.crc_ok);
    }
    if !f.crc_ok {
        return Ok(()); // never act on corrupt payloads
    }
    let Ok(payload) = decode_payload(f.header.ty, &f.payload) else {
        return Ok(());
    };
    let sample = normalize(&payload, spec);
    let source = bolt_codec::frame_source(f.header.ty, &sample);

    let mut l = live.lock().unwrap();
    if let Some(node) = l.nodes.get_mut(source) {
        node.last_seen = Some(now);
        match &sample {
            Sample::Env { .. } => node.env = Some(serde_json::to_value(&sample)?),
            Sample::Status { .. } | Sample::Exp3Status { .. } => {
                node.status = Some(serde_json::to_value(&sample)?);
            }
            Sample::Boot { .. } => {
                let mut v = serde_json::to_value(&sample)?;
                v["seen_utc"] = serde_json::Value::String(now_utc()); // "3 min ago"
                node.boot = Some(v);
            }
            _ => {}
        }
    }
    if let Sample::Status {
        signal: Some(sig), ..
    } = &sample
    {
        l.lo = sig.lo;
        l.sods = sig.sods;
        l.soe = sig.soe;
    }
    drop(l);

    match &sample {
        Sample::CmdAck {
            command, result, ..
        } => {
            if *command == "full_system_test" && *result == "accepted" {
                let id = tracker
                    .lock()
                    .unwrap()
                    .on_ack(&store.lock().unwrap(), &now_utc(), now)?;
                eprintln!("station: full-system test run {id} started");
            }
        }
        Sample::Test {
            node,
            test_id,
            result_name,
            last,
            data,
            ..
        } => {
            tracker.lock().unwrap().on_test(
                &store.lock().unwrap(),
                node,
                u32::from(*test_id),
                result_name,
                i64::from(*data),
                f.header.timestamp_us,
                *last,
                &now_utc(),
                now,
            )?;
        }
        Sample::Gap { reason, node, .. } if *reason == "self_test_skipped" => {
            tracker
                .lock()
                .unwrap()
                .on_node_dark(&store.lock().unwrap(), node, &now_utc())?;
        }
        _ => {}
    }
    Ok(())
}
