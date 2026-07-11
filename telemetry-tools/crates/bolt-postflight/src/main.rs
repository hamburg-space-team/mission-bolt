mod hdf5_writer;
mod manifest;

use std::io::Write;

use anyhow::{Context, Result};
use bolt_codec::{decode_payload, normalize, FrameEvent, Framer, MissionSpec, PayloadType};
use clap::Parser;

use hdf5_writer::HdfCollector;
use manifest::Builder;

#[derive(Parser)]
#[command(name = "bolt-postflight", version, about)]
struct Args {
    // Recorded raw downlink capture
    input: Option<String>,

    // SQLite cache: in decode mode, every packet is written here
    #[arg(long)]
    db: Option<String>,

    // Query mode: read packets from --db as JSON lines on stdout instead of decoding a raw 
    #[arg(long)]
    query: bool,

    // Query filter: only this payload type (e.g. btc_env). Omit for all.
    #[arg(long = "type")]
    ty: Option<String>,

    // Query filter: timestamp_us >= this.
    #[arg(long)]
    since_us: Option<i64>,

    // Query filter: timestamp_us <= this.
    #[arg(long)]
    until_us: Option<i64>,

    // Query pagination: max rows (default 100000).
    #[arg(long)]
    limit: Option<i64>,

    // Query pagination: rows to skip (default 0).
    #[arg(long)]
    offset: Option<i64>,

    // Manifest output path (light index, always written).
    #[arg(long, default_value = "flight.manifest.json")]
    manifest: String,

    // HDF5 output path (heavy arrays). Requires the `hdf5` build feature.
    #[arg(long)]
    hdf5: Option<String>,

    // Mission spec id (pins layout + calibration).
    #[arg(long, default_value = "bolt")]
    mission: String,

    // Export one payload type (e.g. btc_env, exp1_spectrum_a) to CSV.
    #[arg(long)]
    csv: Option<String>,

    // CSV output path (defaults to <csv>.csv).
    #[arg(long)]
    csv_out: Option<String>,

    // Dump every packet of this type as JSON lines (for a formatted table).
    #[arg(long)]
    dump: Option<String>,

    // Dump output path (defaults to <dump>.jsonl).
    #[arg(long)]
    dump_out: Option<String>,

    // Dump EVERY packet (all types) as JSON lines - for the panel browser.
    #[arg(long)]
    dump_all: Option<String>,
}

fn main() -> Result<()> {
    let args = Args::parse();

    // Query mode: serve an already-built cache DB, no decode.
    if args.query {
        return run_query(&args);
    }

    let spec: &'static MissionSpec =
        bolt_codec::find_mission(&args.mission).unwrap_or_else(bolt_codec::default_mission);

    let input = args
        .input
        .as_deref()
        .context("input .raw is required (or pass --query to read a --db)")?;
    let bytes = std::fs::read(input).with_context(|| format!("read {input}"))?;
    eprintln!("[postflight] {} bytes, mission {}", bytes.len(), spec.id);

    let mut mb = Builder::new(spec.id, spec.protocol_version);
    let mut hdf = HdfCollector::new();
    let mut csv_rows: Vec<String> = Vec::new();
    let mut dump_lines: Vec<String> = Vec::new();
    let mut dump_all_lines: Vec<String> = Vec::new();

    // Cache DB: one indexed row per packet. Bulk-load in a single transaction
    // with durability off (a cache is disposable and rebuilt from the raw).
    let mut db = match &args.db {
        Some(path) => Some(DbWriter::create(path)?),
        None => None,
    };

    for ev in Framer::new(&bytes) {
        match ev {
            FrameEvent::Resync { skipped } => mb.add_resync(skipped),
            FrameEvent::Frame(f) => {
                let Ok(payload) = decode_payload(f.header.ty, &f.payload) else {
                    continue; // unknown type: counted as resync noise, skip
                };
                let sample = normalize(&payload, spec);
                let name = PayloadType::from_u8(f.header.ty).map_or("unknown", PayloadType::name);
                let source =
                    PayloadType::from_u8(f.header.ty).map_or("system", PayloadType::source);
                mb.add_frame(
                    f.header.ty,
                    f.header.tick,
                    f.header.timestamp_us,
                    f.crc_ok,
                    &payload,
                    &sample,
                );
                if args.hdf5.is_some() {
                    hdf.add(name, source, f.header.tick, f.header.timestamp_us, &sample);
                }
                if args.csv.as_deref() == Some(name) {
                    // Lossless: real values (never blanked) + validity tags.
                    let cols = sample.raw_columns();
                    if csv_rows.is_empty() {
                        let mut header = String::from("tick,timestamp_us,seq,crc_ok,suspect");
                        for (n, _) in &cols {
                            header.push(',');
                            header.push_str(n);
                        }
                        csv_rows.push(header);
                    }
                    let mut row = format!(
                        "{},{},{},{},{}",
                        f.header.tick,
                        f.header.timestamp_us,
                        f.header.sequence,
                        u8::from(f.crc_ok),
                        u8::from(sample.suspect()),
                    );
                    for (_, v) in &cols {
                        row.push(',');
                        row.push_str(&v.to_string());
                    }
                    csv_rows.push(row);
                }
                if args.dump.is_some() || args.dump_all.is_some() || db.is_some() {
                    let rec = serde_json::json!({
                        "seq": f.header.sequence,
                        "tick": f.header.tick,
                        "timestamp_us": f.header.timestamp_us,
                        "crc_ok": f.crc_ok,
                        "suspect": sample.suspect(),
                        "name": name,
                        "source": source,
                        "sample": &sample,
                    })
                    .to_string();
                    if let Some(w) = db.as_mut() {
                        w.insert(
                            f.header.sequence,
                            f.header.tick,
                            f.header.timestamp_us,
                            f.crc_ok,
                            sample.suspect(),
                            name,
                            source,
                            &rec,
                        )?;
                    }
                    if args.dump.as_deref() == Some(name) {
                        dump_lines.push(rec.clone());
                    }
                    if args.dump_all.is_some() {
                        dump_all_lines.push(rec);
                    }
                }
            }
        }
    }

    let man = mb.finish();
    eprintln!(
        "[postflight] {} frames, {} crc-fails, {} experiments, LO rtc={}",
        man.total_frames,
        man.crc_fail_total,
        man.experiments.len(),
        man.lo_rtc_s
    );

    let json = serde_json::to_string_pretty(&man)?;
    std::fs::File::create(&args.manifest)
        .with_context(|| format!("create {}", args.manifest))?
        .write_all(json.as_bytes())?;
    eprintln!("[postflight] wrote {}", args.manifest);

    if let Some(h5) = &args.hdf5 {
        hdf.write(h5).with_context(|| format!("write {h5}"))?;
        eprintln!("[postflight] wrote {h5}");
    }

    if let Some(target) = &args.csv {
        if csv_rows.is_empty() {
            eprintln!("[postflight] no '{target}' packets found - CSV not written");
        } else {
            let out = args.csv_out.clone().unwrap_or_else(|| format!("{target}.csv"));
            std::fs::write(&out, csv_rows.join("\n")).with_context(|| format!("write {out}"))?;
            eprintln!("[postflight] wrote {out} ({} rows)", csv_rows.len() - 1);
        }
    }

    if let Some(target) = &args.dump {
        let out = args.dump_out.clone().unwrap_or_else(|| format!("{target}.jsonl"));
        std::fs::write(&out, dump_lines.join("\n")).with_context(|| format!("write {out}"))?;
        eprintln!("[postflight] wrote {out} ({} packets)", dump_lines.len());
    }

    if let Some(out) = &args.dump_all {
        std::fs::write(out, dump_all_lines.join("\n")).with_context(|| format!("write {out}"))?;
        eprintln!("[postflight] wrote {out} ({} packets, all types)", dump_all_lines.len());
    }

    if let Some(w) = db.take() {
        let n = w.finish()?;
        eprintln!("[postflight] wrote {} ({n} packets indexed)", args.db.as_deref().unwrap_or("db"));
    }

    Ok(())
}

// Bulk loader for the packet cache DB
struct DbWriter {
    conn: rusqlite::Connection,
    count: u64,
}

impl DbWriter {
    fn create(path: &str) -> Result<Self> {
        let _ = std::fs::remove_file(path);
        let conn = rusqlite::Connection::open(path).with_context(|| format!("open db {path}"))?;
        conn.execute_batch(
            "PRAGMA journal_mode = OFF;
             PRAGMA synchronous = OFF;
             CREATE TABLE packets(
                 id            INTEGER PRIMARY KEY,
                 seq           INTEGER,
                 tick          INTEGER,
                 timestamp_us  INTEGER,
                 crc_ok        INTEGER,
                 suspect       INTEGER,
                 name          TEXT,
                 source        TEXT,
                 json          TEXT
             );
             BEGIN;",
        )?;
        Ok(Self { conn, count: 0 })
    }

    #[allow(clippy::too_many_arguments)]
    fn insert(
        &mut self,
        seq: u8,
        tick: u16,
        timestamp_us: u32,
        crc_ok: bool,
        suspect: bool,
        name: &str,
        source: &str,
        json: &str,
    ) -> Result<()> {
        let mut stmt = self.conn.prepare_cached(
            "INSERT INTO packets(seq, tick, timestamp_us, crc_ok, suspect, name, source, json)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)",
        )?;
        stmt.execute(rusqlite::params![
            seq,
            tick,
            timestamp_us,
            u8::from(crc_ok),
            u8::from(suspect),
            name,
            source,
            json,
        ])?;
        self.count += 1;
        Ok(())
    }

    fn finish(self) -> Result<u64> {
        self.conn.execute_batch(
            "COMMIT;
             CREATE INDEX idx_name ON packets(name);
             CREATE INDEX idx_ts   ON packets(timestamp_us);",
        )?;
        Ok(self.count)
    }
}

// Serve the cache DB
fn run_query(args: &Args) -> Result<()> {
    let path = args.db.as_deref().context("--query requires --db <cache.sqlite>")?;

    let conn = rusqlite::Connection::open_with_flags(
        path,
        rusqlite::OpenFlags::SQLITE_OPEN_READ_ONLY,
    )
    .with_context(|| format!("open db {path}"))?;

    let mut clauses: Vec<&str> = Vec::new();
    let mut params: Vec<Box<dyn rusqlite::ToSql>> = Vec::new();

    if let Some(t) = &args.ty {
        clauses.push("name = ?");
        params.push(Box::new(t.clone()));
    }

    if let Some(s) = args.since_us {
        clauses.push("timestamp_us >= ?");
        params.push(Box::new(s));
    }

    if let Some(u) = args.until_us {
        clauses.push("timestamp_us <= ?");
        params.push(Box::new(u));
    }

    let where_ = if clauses.is_empty() {
        String::new()
    } else {
        format!(" WHERE {}", clauses.join(" AND "))
    };
    
    params.push(Box::new(args.limit.unwrap_or(100_000)));
    params.push(Box::new(args.offset.unwrap_or(0)));

    let sql = format!("SELECT json FROM packets{where_} ORDER BY id DESC LIMIT ? OFFSET ?");
    let mut stmt = conn.prepare(&sql)?;
    let refs: Vec<&dyn rusqlite::ToSql> = params.iter().map(|b| &**b as &dyn rusqlite::ToSql).collect();
    let rows = stmt.query_map(refs.as_slice(), |r| r.get::<_, String>(0))?;

    let stdout = std::io::stdout();
    let mut w = std::io::BufWriter::new(stdout.lock());
    for row in rows {
        writeln!(w, "{}", row?)?;
    }
    w.flush()?;
    Ok(())
}
