//! The station's HTTP API and the kiosk dashboard it serves. The flash
//! queue routes are reserved for the four-probe stage and say so.

use std::net::UdpSocket;
use std::sync::{Arc, Mutex};
use std::time::Instant;

use serde_json::json;
use tiny_http::{Header, Method, Response, Server};

use crate::state::{Live, NODES};
use crate::store::Store;
use crate::tracker::{step_label, Tracker};
use crate::{probe, Config};

pub struct Api {
    pub cfg: Config,
    pub live: Arc<Mutex<Live>>,
    pub store: Arc<Mutex<Store>>,
    pub tracker: Arc<Mutex<Tracker>>,
    /// writer back to the RXSM link, for uplink commands
    pub uplink: crate::Uplink,
    /// per-type sequence counter the wire header carries
    pub cmd_seq: Arc<Mutex<u8>>,
    /// where the .noinit marker was last found, so the scan runs once
    pub noinit_at: Arc<Mutex<Option<u32>>>,
    /// the LO line on the ST-Link's bridge GPIO, held across probe replugs
    pub lo: Arc<crate::bridge::LoLine>,
}

/// Content type by extension; the dashboard ships html/js/css only.
fn content_type(path: &str) -> &'static str {
    match path.rsplit('.').next().unwrap_or("") {
        "html" => "text/html; charset=utf-8",
        "js" => "text/javascript; charset=utf-8",
        "css" => "text/css; charset=utf-8",
        "json" => "application/json",
        "svg" => "image/svg+xml",
        "ico" => "image/x-icon",
        _ => "application/octet-stream",
    }
}

/// Serve the kiosk dashboard; unknown paths fall back to index.html.
fn respond_static(api: &Api, req: tiny_http::Request, path: &str) {
    let root = std::path::Path::new(&api.cfg.ui_dir);
    let rel = path.trim_start_matches('/');
    // no traversal: a component-wise check, not a string search
    let safe = !std::path::Path::new(rel)
        .components()
        .any(|c| matches!(c, std::path::Component::ParentDir));
    let mut file = root.join(if rel.is_empty() { "index.html" } else { rel });
    if !safe || !file.is_file() {
        file = root.join("index.html");
    }
    let Ok(body) = std::fs::read(&file) else {
        let _ = req.respond(
            Response::from_string("dashboard not installed - see tools/debug-station/README.md")
                .with_status_code(404),
        );
        return;
    };
    let ct = content_type(file.to_str().unwrap_or(""));
    let _ = req.respond(
        Response::from_data(body)
            .with_header(Header::from_bytes(&b"Content-Type"[..], ct.as_bytes()).unwrap()),
    );
}

/// The address the network reaches us under. A UDP connect sends nothing,
/// it just picks the route's source address; unplugged there is no route,
/// so fall back to whatever an interface still carries.
fn primary_ip() -> Option<String> {
    let routed = UdpSocket::bind("0.0.0.0:0")
        .ok()
        .and_then(|s| {
            s.connect("8.8.8.8:53")
                .ok()
                .and_then(|()| s.local_addr().ok())
        })
        .map(|a| a.ip().to_string());
    routed.or_else(|| {
        interfaces()
            .first()
            .and_then(|i| i["cidr"].as_str())
            .map(|c| c.split('/').next().unwrap_or(c).to_string())
    })
}

/// Every IPv4 address the station holds. iproute2 marks a DHCP-assigned
/// one `dynamic`, so the kernel answers "static or lease", not a guess.
fn interfaces() -> Vec<serde_json::Value> {
    let Ok(out) = std::process::Command::new("ip")
        .args(["-o", "-4", "addr", "show"])
        .output()
    else {
        return Vec::new();
    };
    String::from_utf8_lossy(&out.stdout)
        .lines()
        .filter_map(|line| {
            let f: Vec<&str> = line.split_whitespace().collect();
            let name = f.get(1)?;
            if *name == "lo" {
                return None;
            }
            let cidr = f
                .iter()
                .position(|t| *t == "inet")
                .and_then(|i| f.get(i + 1))?;
            Some(json!({
                "interface": name,
                "cidr": cidr,
                "source": if line.contains(" dynamic ") { "dhcp" } else { "static" },
            }))
        })
        .collect()
}

fn json_response(v: &serde_json::Value, status: u16) -> Response<std::io::Cursor<Vec<u8>>> {
    let body = v.to_string().into_bytes();
    Response::from_data(body)
        .with_status_code(status)
        .with_header(Header::from_bytes(&b"Content-Type"[..], &b"application/json"[..]).unwrap())
}

pub fn serve(api: &Api, listen: &str) -> anyhow::Result<()> {
    let server = Server::http(listen).map_err(|e| anyhow::anyhow!("http {listen}: {e}"))?;
    eprintln!("station: http api on {listen}");
    for req in server.incoming_requests() {
        let path = req.url().split('?').next().unwrap_or("").to_string();
        let query = req.url().split('?').nth(1).unwrap_or("").to_string();
        if !path.starts_with("/api/") {
            respond_static(api, req, &path);
            continue;
        }
        let v = route(api, req.method(), &path, &query);
        let (body, status) = match v {
            Ok(v) => (v, 200),
            Err(e) => (json!({ "error": e.to_string() }), 500),
        };
        let _ = req.respond(json_response(&body, status));
    }
    Ok(())
}

fn route(api: &Api, method: &Method, path: &str, query: &str) -> anyhow::Result<serde_json::Value> {
    match (method, path) {
        (Method::Get, "/api/status") => status(api),
        (Method::Post, "/api/window/reset") => {
            api.live.lock().unwrap().window.reset(Instant::now());
            Ok(json!({ "ok": true }))
        }
        (Method::Get, "/api/env") => {
            let live = api.live.lock().unwrap();
            let mut out = serde_json::Map::new();
            for n in NODES {
                out.insert(n.into(), live.nodes[n].env.clone().unwrap_or(json!(null)));
            }
            Ok(json!(out))
        }
        (Method::Get, "/api/tests") => {
            let runs = api.store.lock().unwrap().runs(100)?;
            let active = api.tracker.lock().unwrap().active_run_id();
            Ok(json!({ "active_run": active, "runs": runs }))
        }
        (Method::Get, p) if p.starts_with("/api/tests/") => {
            let id: i64 = p["/api/tests/".len()..].parse()?;
            run_detail(api, id)
        }
        (Method::Get, "/api/lo") => lo(api, None),
        (Method::Post, "/api/lo") => {
            // no level given means the one there is a reason to ask for
            let want = query
                .split('&')
                .find_map(|kv| kv.strip_prefix("level="))
                .unwrap_or("high");
            lo(api, Some(want))
        }
        (Method::Get, "/api/debugger") => debugger(api),
        (Method::Get, "/api/debugger/noinit") => noinit(api, query),
        (Method::Get, "/api/selftest/steps") => Ok(selftest_steps()),
        (Method::Post, p) if p.starts_with("/api/command/") => {
            send_command(api, &p["/api/command/".len()..])
        }
        // reserved for the four-probe stage: upload queue, per-board binary
        // store, flash + auto full-system-test. Shape is settled, code is not
        (_, p) if p.starts_with("/api/queue") => Ok(json!({
            "error": "flash queue is not implemented yet (single-probe stage)",
            "planned": ["POST /api/queue/<board> (binary)", "GET /api/queue",
                        "POST /api/queue/pause", "POST /api/queue/resume",
                        "POST /api/queue/retry/<board>"],
        })),
        _ => Ok(json!({ "error": "unknown route" })),
    }
}

fn status(api: &Api) -> anyhow::Result<serde_json::Value> {
    let now = Instant::now();
    let mut live = api.live.lock().unwrap();
    let (win_s, win_frames, win_fails, win_bytes) = live.window.summary(now);
    // rate over what the window covers, not over ten minutes it never saw
    let secs = win_s.max(1) as f64;
    let boards: serde_json::Map<String, serde_json::Value> = NODES
        .iter()
        .map(|n| {
            let node = &live.nodes[n];
            let mode = node
                .status
                .as_ref()
                .and_then(|s| s.get("mode").cloned())
                .unwrap_or(json!(null));
            (
                (*n).to_string(),
                json!({
                    "sending": live.sending(n, now),
                    "last_seen_ms_ago": node.last_seen.map(|t| now.duration_since(t).as_millis() as u64),
                    "mode": mode,
                    "env": node.env,
                    "status": node.status,
                    "boot": node.boot,
                }),
            )
        })
        .collect();

    Ok(json!({
        "station": {
            "ip": primary_ip(),
            "hostname": std::fs::read_to_string("/etc/hostname").map(|h| h.trim().to_string()).ok(),
            "interfaces": interfaces(),
        },
        "signals": { "lo": live.lo, "sods": live.sods, "soe": live.soe },
        "link": {
            "total_frames": live.total_frames,
            "total_crc_fails": live.total_crc_fail,
            "total_bytes": live.total_bytes,
            // 8N1: ten line bits per byte.
            "baud": api.cfg.baud,
            "link_bps": api.cfg.baud / 10,
            "budget_bps": api.cfg.flight_baud / 10,
            "flight_baud": api.cfg.flight_baud,
            "window": {
                "seconds": win_s,
                "frames": win_frames,
                "crc_fails": win_fails,
                "bytes": win_bytes,
                "frames_per_s": (win_frames as f64 / secs * 10.0).round() / 10.0,
                "bytes_per_s": (win_bytes as f64 / secs).round(),
            },
        },
        "boards": boards,
        "probe": probe_summary(api),
        "active_run": api.tracker.lock().unwrap().active_run_id(),
    }))
}

fn probe_summary(api: &Api) -> serde_json::Value {
    let usb = probe::stlink_on_usb();
    // board identity only when the probe answers; a dead target is not an error
    let board = if usb {
        probe::read_words(&api.cfg.openocd_tcl, probe::UID_ADDR, 3)
            .ok()
            .map(|w| {
                let uid = probe::uid_string(&w);
                let map = probe::load_uid_map(&api.cfg.uid_map);
                let entry = probe::entry_for_uid(&map, &uid);
                json!({
                    "uid": uid,
                    "board": entry.map(|e| e.board.clone()),
                })
            })
    } else {
        None
    };
    json!({ "stlink_on_usb": usb, "target": board })
}

/// The board the probe currently sits on, and its .noinit address if
/// uids.conf carries one.
fn probe_board(api: &Api) -> Option<probe::BoardEntry> {
    let w = probe::read_words(&api.cfg.openocd_tcl, probe::UID_ADDR, 3).ok()?;
    let uid = probe::uid_string(&w);
    let map = probe::load_uid_map(&api.cfg.uid_map);
    probe::entry_for_uid(&map, &uid).cloned()
}

/// Per-node self-test step names from the wire contract, so no consumer
/// keeps a list of its own.
fn selftest_steps() -> serde_json::Value {
    let map: serde_json::Map<String, serde_json::Value> = bolt_codec::SELF_TEST_STEPS
        .iter()
        .map(|(node, steps)| ((*node).to_string(), json!(steps)))
        .collect();
    json!(map)
}

/// Encode an uplink command out the serial port the station owns. The
/// contract's danger flag rides along; confirming is the caller's job.
fn send_command(api: &Api, name: &str) -> anyhow::Result<serde_json::Value> {
    let Some(cmd) = bolt_codec::Command::from_name(name) else {
        anyhow::bail!("unknown command '{name}'");
    };
    let mut guard = api.uplink.lock().unwrap();
    let Some(writer) = guard.as_mut() else {
        anyhow::bail!("no live byte source - the station has no serial port open");
    };
    let mut seq = api.cmd_seq.lock().unwrap();
    let frame = bolt_codec::encode(&cmd, &mut seq);
    writer.write_all(&frame)?;
    writer.flush()?;
    Ok(json!({
        "sent": name,
        "opcode": cmd.opcode(),
        "dangerous": cmd.is_dangerous(),
        "bytes": frame.len(),
    }))
}

/// Read or drive the LO line on the ST-Link's bridge GPIO. `want` is None
/// for a plain read. A level that was asked for is re-applied whenever the
/// probe comes back, so it survives a replug.
fn lo(api: &Api, want: Option<&str>) -> anyhow::Result<serde_json::Value> {
    let high = match want {
        None => None,
        Some("high" | "1" | "on" | "true") => Some(true),
        Some("low" | "0" | "off" | "false") => Some(false),
        Some(other) => anyhow::bail!("level must be high or low, not '{other}'"),
    };
    let level = match high {
        Some(h) => api.lo.set(h)?,
        None => api.lo.get()?,
    };
    Ok(json!({
        "gpio": api.lo.gpio(),
        "level": if level { "high" } else { "low" },
        "driven": high.is_some(),
        "idle": api.cfg.lo_idle.map(|l| if l == crate::Level::High { "high" } else { "low" }),
    }))
}

fn debugger(api: &Api) -> anyhow::Result<serde_json::Value> {
    let map = probe::load_uid_map(&api.cfg.uid_map);
    // one chip cannot be two boards, and the lookup would quietly answer
    // with whichever came first
    let mut warnings: Vec<String> = Vec::new();
    for (i, e) in map.iter().enumerate() {
        if let Some(first) = map.iter().take(i).find(|o| o.uid == e.uid) {
            warnings.push(format!(
                "uid {} is mapped twice ('{}' and '{}') - only '{}' takes effect",
                e.uid, first.board, e.board, first.board
            ));
        }
    }
    Ok(json!({
        "probe": probe_summary(api),
        "openocd_tcl": api.cfg.openocd_tcl,
        "uid_map_file": api.cfg.uid_map,
        "mapped": map.iter().map(|e| json!({"uid": e.uid, "board": e.board})).collect::<Vec<_>>(),
        "config_warnings": warnings,
        "hint": "map a new board: put the probe on it, read uid here, append '<uid> <board>'",
    }))
}

fn noinit(api: &Api, query: &str) -> anyhow::Result<serde_json::Value> {
    // ?addr= wins, then uids.conf, then config, then the scan. "no state"
    // is an answer, not an error: it differs from "no probe"
    let explicit = query
        .split('&')
        .find_map(|kv| kv.strip_prefix("addr="))
        .map(|v| u32::from_str_radix(v.trim_start_matches("0x"), 16))
        .transpose()?
        .or_else(|| probe_board(api).and_then(|e| e.noinit))
        .or(api.cfg.noinit_addr);

    // remember what the scan found
    let cached = *api.noinit_at.lock().unwrap();
    let addr = match explicit.or(cached) {
        Some(a) => Some(a),
        None => {
            let found = probe::find_noinit(&api.cfg.openocd_tcl);
            if let Some(a) = found {
                *api.noinit_at.lock().unwrap() = Some(a);
            }
            found
        }
    };

    let Some(addr) = addr else {
        return Ok(json!({
            "valid": false,
            "probe_reachable": probe::read_words(&api.cfg.openocd_tcl, probe::UID_ADDR, 1).is_ok(),
            "note": "no BootState magic anywhere in the scanned SRAM - the board cold-booted \
                     (flash.sh clears it), or the probe cannot reach it",
        }));
    };

    let words = probe::read_words(&api.cfg.openocd_tcl, addr, probe::NOINIT_WORDS)?;
    let decoded = probe::decode_noinit(addr, &words);
    // a moved image invalidates the remembered address; rescan next call
    if decoded["valid"] == json!(false) {
        *api.noinit_at.lock().unwrap() = None;
    }
    Ok(decoded)
}

fn run_detail(api: &Api, id: i64) -> anyhow::Result<serde_json::Value> {
    let rows = api.store.lock().unwrap().run_results(id)?;
    let mut by_node: serde_json::Map<String, serde_json::Value> = serde_json::Map::new();
    for n in NODES {
        let steps: Vec<serde_json::Value> = rows
            .iter()
            .filter(|r| r.node == n)
            .map(|r| {
                json!({
                    "test_id": r.test_id,
                    "name": step_label(n, r.test_id).unwrap_or("(newer than this build)"),
                    "result": r.result,
                    "data": r.data,
                    "timestamp_us": r.timestamp_us,
                    "recv_utc": r.recv_utc,
                    "duration_ms": r.duration_ms,
                })
            })
            .collect();
        by_node.insert(n.into(), json!(steps));
    }
    let pass = rows.iter().filter(|r| r.result == "pass").count();
    let fail = rows.iter().filter(|r| r.result == "fail").count();
    Ok(json!({ "run": id, "pass": pass, "fail": fail, "nodes": by_node }))
}
