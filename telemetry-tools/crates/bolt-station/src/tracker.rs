//! Full-system-test runs, driven by the decoded frame stream: the ack
//! opens one, *_TEST reports fill it, a skip GAP_MARKER darkens a node,
//! and it closes once every node is accounted for.

use std::collections::HashMap;
use std::time::Instant;

use crate::state::NODES;
use crate::store::{ResultRow, Store};

/// Step counts per node from the generated wire contract.
fn step_count(node: &str) -> usize {
    bolt_codec::SELF_TEST_STEPS
        .iter()
        .find(|(n, _)| *n == node)
        .map_or(0, |(_, steps)| steps.len())
}

pub fn step_label(node: &str, test_id: u32) -> Option<&'static str> {
    bolt_codec::SELF_TEST_STEPS
        .iter()
        .find(|(n, _)| *n == node)
        .and_then(|(_, steps)| steps.get(test_id as usize).copied())
}

struct NodeProgress {
    reported: Vec<bool>,
    last_ts_us: Option<u32>,
    done: bool,
    dark: bool,
}

pub struct ActiveRun {
    pub id: i64,
    nodes: HashMap<&'static str, NodeProgress>,
    last_report: Instant,
}

/// What the tracker asks its caller to persist / update.
pub struct Tracker {
    active: Option<ActiveRun>,
}

const SILENCE_TIMEOUT_S: u64 = 120;

impl Tracker {
    pub fn new() -> Self {
        Self { active: None }
    }

    pub fn active_run_id(&self) -> Option<i64> {
        self.active.as_ref().map(|r| r.id)
    }

    /// FULL_SYSTEM_TEST acknowledged: close whatever ran, open a fresh run.
    pub fn on_ack(&mut self, store: &Store, now_utc: &str, now: Instant) -> anyhow::Result<i64> {
        if let Some(run) = self.active.take() {
            store.finish_run(run.id, now_utc)?;
        }
        let id = store.start_run(now_utc)?;
        let mut nodes = HashMap::new();
        for n in NODES {
            nodes.insert(
                n,
                NodeProgress {
                    reported: vec![false; step_count(n)],
                    last_ts_us: None,
                    done: false,
                    dark: false,
                },
            );
        }
        self.active = Some(ActiveRun {
            id,
            nodes,
            last_report: now,
        });
        Ok(id)
    }

    /// One *_TEST report. `result` is the codec's name (pass/fail/skipped).
    #[allow(clippy::too_many_arguments)]
    pub fn on_test(
        &mut self,
        store: &Store,
        node: &str,
        test_id: u32,
        result: &str,
        data: i64,
        timestamp_us: u32,
        last: bool,
        now_utc: &str,
        now: Instant,
    ) -> anyhow::Result<()> {
        let Some(run) = self.active.as_mut() else {
            return Ok(()); // a run we did not see the ack of - ignore
        };
        run.last_report = now;
        let Some(progress) = run.nodes.get_mut(node) else {
            return Ok(());
        };

        // gap to the node's previous report, wrap-safe in the us domain
        let duration_ms = progress
            .last_ts_us
            .map(|prev| f64::from(timestamp_us.wrapping_sub(prev)) / 1000.0);
        progress.last_ts_us = Some(timestamp_us);
        if let Some(r) = progress.reported.get_mut(test_id as usize) {
            *r = true;
        }

        store.record(
            run.id,
            &ResultRow {
                node: node.to_string(),
                test_id,
                result: if result == "skipped" { "skip" } else { result }.to_string(),
                data: Some(data),
                timestamp_us: Some(i64::from(timestamp_us)),
                recv_utc: now_utc.to_string(),
                duration_ms,
            },
        )?;

        if last {
            progress.done = true;
            // ids that never arrived although the run moved past them
            let missing: Vec<u32> = progress
                .reported
                .iter()
                .enumerate()
                .filter(|(_, seen)| !**seen)
                .map(|(i, _)| i as u32)
                .collect();
            for id in missing {
                store.record(
                    run.id,
                    &ResultRow {
                        node: node.to_string(),
                        test_id: id,
                        result: "no_report".to_string(),
                        data: None,
                        timestamp_us: None,
                        recv_utc: now_utc.to_string(),
                        duration_ms: None,
                    },
                )?;
            }
        }
        self.maybe_finish(store, now_utc)?;
        Ok(())
    }

    /// GAP_MARKER self_test_skipped: the BTC gave up on this node.
    pub fn on_node_dark(&mut self, store: &Store, node: &str, now_utc: &str) -> anyhow::Result<()> {
        let Some(run) = self.active.as_mut() else {
            return Ok(());
        };
        let Some(progress) = run.nodes.get_mut(node) else {
            return Ok(());
        };
        progress.dark = true;
        progress.done = true;
        let missing: Vec<u32> = progress
            .reported
            .iter()
            .enumerate()
            .filter(|(_, seen)| !**seen)
            .map(|(i, _)| i as u32)
            .collect();
        for id in missing {
            store.record(
                run.id,
                &ResultRow {
                    node: node.to_string(),
                    test_id: id,
                    result: "node_dark".to_string(),
                    data: None,
                    timestamp_us: None,
                    recv_utc: now_utc.to_string(),
                    duration_ms: None,
                },
            )?;
        }
        self.maybe_finish(store, now_utc)?;
        Ok(())
    }

    /// Close a stalled run so the next ack does not inherit it.
    pub fn on_idle_tick(
        &mut self,
        store: &Store,
        now_utc: &str,
        now: Instant,
    ) -> anyhow::Result<()> {
        let stalled = self
            .active
            .as_ref()
            .is_some_and(|r| now.duration_since(r.last_report).as_secs() >= SILENCE_TIMEOUT_S);
        if stalled {
            if let Some(run) = self.active.take() {
                store.finish_run(run.id, now_utc)?;
            }
        }
        Ok(())
    }

    fn maybe_finish(&mut self, store: &Store, now_utc: &str) -> anyhow::Result<()> {
        let all_done = self
            .active
            .as_ref()
            .is_some_and(|r| r.nodes.values().all(|n| n.done));
        if all_done {
            if let Some(run) = self.active.take() {
                store.finish_run(run.id, now_utc)?;
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn full_node(t: &mut Tracker, s: &Store, node: &str, now: Instant) {
        let n = step_count(node) as u32;
        for id in 0..n {
            t.on_test(
                s,
                node,
                id,
                "pass",
                1,
                1000 * (id + 1),
                id == n - 1,
                "t",
                now,
            )
            .unwrap();
        }
    }

    #[test]
    fn full_run_opens_records_and_closes() {
        let s = Store::open(":memory:").unwrap();
        let mut t = Tracker::new();
        let now = Instant::now();
        t.on_ack(&s, "t0", now).unwrap();
        assert!(t.active_run_id().is_some());

        for node in NODES {
            full_node(&mut t, &s, node, now);
        }
        assert!(t.active_run_id().is_none(), "all nodes done closes the run");
        let runs = s.runs(10).unwrap();
        assert!(runs[0].finished_utc.is_some());
        let expected: i64 = NODES.iter().map(|n| step_count(n) as i64).sum();
        assert_eq!(runs[0].pass, expected);
    }

    #[test]
    fn dark_node_rows_fill_in_and_do_not_count() {
        let s = Store::open(":memory:").unwrap();
        let mut t = Tracker::new();
        let now = Instant::now();
        t.on_ack(&s, "t0", now).unwrap();
        for node in ["btc", "exp1", "exp3"] {
            full_node(&mut t, &s, node, now);
        }
        t.on_node_dark(&s, "exp2", "t1").unwrap();
        assert!(t.active_run_id().is_none());

        let runs = s.runs(10).unwrap();
        let dark_rows = s
            .run_results(runs[0].id)
            .unwrap()
            .into_iter()
            .filter(|r| r.result == "node_dark")
            .count();
        assert_eq!(dark_rows, step_count("exp2"));
        let expected: i64 = ["btc", "exp1", "exp3"]
            .iter()
            .map(|n| step_count(n) as i64)
            .sum();
        assert_eq!(runs[0].pass, expected, "dark rows stay out of the totals");
    }

    #[test]
    fn lost_reports_are_marked_when_last_arrives() {
        let s = Store::open(":memory:").unwrap();
        let mut t = Tracker::new();
        let now = Instant::now();
        t.on_ack(&s, "t0", now).unwrap();
        // only the final btc step arrives; everything before it was lost
        let n = step_count("btc") as u32;
        t.on_test(&s, "btc", n - 1, "pass", 0, 99, true, "t", now)
            .unwrap();
        let runs = s.runs(10).unwrap();
        let rows = s.run_results(runs[0].id).unwrap();
        let lost = rows.iter().filter(|r| r.result == "no_report").count();
        assert_eq!(lost, step_count("btc") - 1);
    }

    #[test]
    fn a_stalled_run_times_out() {
        let s = Store::open(":memory:").unwrap();
        let mut t = Tracker::new();
        let t0 = Instant::now();
        t.on_ack(&s, "t0", t0).unwrap();
        t.on_idle_tick(&s, "t1", t0 + std::time::Duration::from_secs(121))
            .unwrap();
        assert!(t.active_run_id().is_none());
    }
}
