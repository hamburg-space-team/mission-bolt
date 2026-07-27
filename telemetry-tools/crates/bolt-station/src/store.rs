//! Test-run persistence, keyed (run_id, node, test_id). New or reordered
//! firmware steps can only add rows, never migrate the schema.

use anyhow::{Context, Result};
use rusqlite::Connection;

pub struct Store {
    conn: Connection,
}

/// One step report as it goes into (and comes out of) the database.
#[derive(Debug, Clone, serde::Serialize)]
pub struct ResultRow {
    pub node: String,
    pub test_id: u32,
    pub result: String, // pass / fail / skip / no_report / node_dark
    pub data: Option<i64>,
    pub timestamp_us: Option<i64>,
    pub recv_utc: String,
    pub duration_ms: Option<f64>,
}

#[derive(Debug, Clone, serde::Serialize)]
pub struct RunSummary {
    pub id: i64,
    pub started_utc: String,
    pub finished_utc: Option<String>,
    /// pass/fail only - skips, lost reports and dark nodes are not results
    pub pass: i64,
    pub fail: i64,
}

impl Store {
    pub fn open(path: &str) -> Result<Self> {
        let conn = if path == ":memory:" {
            Connection::open_in_memory()?
        } else {
            Connection::open(path).with_context(|| format!("open {path}"))?
        };
        conn.execute_batch(
            "CREATE TABLE IF NOT EXISTS runs (
                 id INTEGER PRIMARY KEY AUTOINCREMENT,
                 started_utc TEXT NOT NULL,
                 finished_utc TEXT
             );
             CREATE TABLE IF NOT EXISTS results (
                 run_id INTEGER NOT NULL,
                 node TEXT NOT NULL,
                 test_id INTEGER NOT NULL,
                 result TEXT NOT NULL,
                 data INTEGER,
                 timestamp_us INTEGER,
                 recv_utc TEXT NOT NULL,
                 duration_ms REAL,
                 PRIMARY KEY (run_id, node, test_id)
             );",
        )?;
        Ok(Self { conn })
    }

    pub fn start_run(&self, started_utc: &str) -> Result<i64> {
        self.conn
            .execute("INSERT INTO runs (started_utc) VALUES (?1)", [started_utc])?;
        Ok(self.conn.last_insert_rowid())
    }

    pub fn finish_run(&self, id: i64, finished_utc: &str) -> Result<()> {
        self.conn.execute(
            "UPDATE runs SET finished_utc = ?1 WHERE id = ?2",
            rusqlite::params![finished_utc, id],
        )?;
        Ok(())
    }

    pub fn record(&self, run_id: i64, r: &ResultRow) -> Result<()> {
        self.conn.execute(
            "INSERT OR REPLACE INTO results
             (run_id, node, test_id, result, data, timestamp_us, recv_utc, duration_ms)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)",
            rusqlite::params![
                run_id,
                r.node,
                r.test_id,
                r.result,
                r.data,
                r.timestamp_us,
                r.recv_utc,
                r.duration_ms
            ],
        )?;
        Ok(())
    }

    pub fn runs(&self, limit: u32) -> Result<Vec<RunSummary>> {
        let mut stmt = self.conn.prepare(
            "SELECT r.id, r.started_utc, r.finished_utc,
                    COALESCE(SUM(res.result = 'pass'), 0),
                    COALESCE(SUM(res.result = 'fail'), 0)
             FROM runs r LEFT JOIN results res ON res.run_id = r.id
             GROUP BY r.id ORDER BY r.id DESC LIMIT ?1",
        )?;
        let rows = stmt
            .query_map([limit], |row| {
                Ok(RunSummary {
                    id: row.get(0)?,
                    started_utc: row.get(1)?,
                    finished_utc: row.get(2)?,
                    pass: row.get(3)?,
                    fail: row.get(4)?,
                })
            })?
            .collect::<rusqlite::Result<Vec<_>>>()?;
        Ok(rows)
    }

    pub fn run_results(&self, run_id: i64) -> Result<Vec<ResultRow>> {
        let mut stmt = self.conn.prepare(
            "SELECT node, test_id, result, data, timestamp_us, recv_utc, duration_ms
             FROM results WHERE run_id = ?1 ORDER BY node, test_id",
        )?;
        let rows = stmt
            .query_map([run_id], |row| {
                Ok(ResultRow {
                    node: row.get(0)?,
                    test_id: row.get(1)?,
                    result: row.get(2)?,
                    data: row.get(3)?,
                    timestamp_us: row.get(4)?,
                    recv_utc: row.get(5)?,
                    duration_ms: row.get(6)?,
                })
            })?
            .collect::<rusqlite::Result<Vec<_>>>()?;
        Ok(rows)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn row(node: &str, id: u32, result: &str) -> ResultRow {
        ResultRow {
            node: node.into(),
            test_id: id,
            result: result.into(),
            data: Some(7),
            timestamp_us: Some(1000),
            recv_utc: "2026-07-27T12:00:00Z".into(),
            duration_ms: Some(40.0),
        }
    }

    #[test]
    fn totals_count_pass_fail_only() {
        let s = Store::open(":memory:").unwrap();
        let run = s.start_run("2026-07-27T12:00:00Z").unwrap();
        s.record(run, &row("btc", 0, "pass")).unwrap();
        s.record(run, &row("btc", 1, "fail")).unwrap();
        s.record(run, &row("btc", 2, "skip")).unwrap();
        s.record(run, &row("exp2", 0, "node_dark")).unwrap();

        let runs = s.runs(10).unwrap();
        assert_eq!(runs.len(), 1);
        assert_eq!((runs[0].pass, runs[0].fail), (1, 1));
    }

    #[test]
    fn a_new_test_id_is_just_a_new_row() {
        let s = Store::open(":memory:").unwrap();
        let run = s.start_run("t").unwrap();
        // a test_id far beyond today's tables must not need a migration
        s.record(run, &row("exp2", 250, "pass")).unwrap();
        assert_eq!(s.run_results(run).unwrap().len(), 1);
    }

    #[test]
    fn duplicate_report_replaces_not_duplicates() {
        let s = Store::open(":memory:").unwrap();
        let run = s.start_run("t").unwrap();
        s.record(run, &row("btc", 0, "fail")).unwrap();
        s.record(run, &row("btc", 0, "pass")).unwrap();
        let rows = s.run_results(run).unwrap();
        assert_eq!(rows.len(), 1);
        assert_eq!(rows[0].result, "pass");
    }
}
