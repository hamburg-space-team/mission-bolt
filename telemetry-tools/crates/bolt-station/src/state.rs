//! Live state the decode thread fills and the HTTP thread reads.

use std::collections::HashMap;
use std::time::Instant;

pub const NODES: [&str; 4] = ["btc", "exp1", "exp2", "exp3"];

/// 10-minute link-quality window as 60 x 10 s buckets. The reset endpoint
/// starts a fresh measurement without touching the all-time counters.
pub struct LinkWindow {
    buckets: [(u64, u64, u64); 60], // (frames, crc_fails, bytes)
    head: usize,
    head_started: Instant,
    started: Instant,
}

impl LinkWindow {
    pub fn new(now: Instant) -> Self {
        Self {
            buckets: [(0, 0, 0); 60],
            head: 0,
            head_started: now,
            started: now,
        }
    }

    fn rotate(&mut self, now: Instant) {
        // advance the head bucket every 10 s, zeroing what it overwrites
        while now.duration_since(self.head_started).as_secs() >= 10 {
            self.head = (self.head + 1) % self.buckets.len();
            self.buckets[self.head] = (0, 0, 0);
            self.head_started += std::time::Duration::from_secs(10);
        }
    }

    pub fn record(&mut self, now: Instant, crc_ok: bool) {
        self.rotate(now);
        let b = &mut self.buckets[self.head];
        b.0 += 1;
        if !crc_ok {
            b.1 += 1;
        }
    }

    /// Raw bytes where they arrive, so framing and resync are in the rate
    pub fn record_bytes(&mut self, now: Instant, n: u64) {
        self.rotate(now);
        self.buckets[self.head].2 += n;
    }

    pub fn reset(&mut self, now: Instant) {
        *self = Self::new(now);
    }

    /// (covered seconds, frames, crc fails, bytes) of the current window
    pub fn summary(&mut self, now: Instant) -> (u64, u64, u64, u64) {
        self.rotate(now);
        let covered = now.duration_since(self.started).as_secs().min(600);
        let (mut f, mut c, mut b) = (0, 0, 0);
        for (frames, fails, bytes) in self.buckets {
            f += frames;
            c += fails;
            b += bytes;
        }
        (covered, f, c, b)
    }
}

/// Latest decoded samples per node plus freshness.
#[derive(Default)]
pub struct NodeState {
    pub env: Option<serde_json::Value>,
    pub status: Option<serde_json::Value>,
    /// Last BOOT packet: reason and reboot count for every node, no probe
    /// needed. Only from when the station was listening - a node sends it
    /// once
    pub boot: Option<serde_json::Value>,
    pub last_seen: Option<Instant>,
}

pub struct Live {
    pub nodes: HashMap<&'static str, NodeState>,
    pub lo: bool,
    pub sods: bool,
    pub soe: bool,
    pub total_frames: u64,
    pub total_crc_fail: u64,
    pub total_bytes: u64,
    pub window: LinkWindow,
}

impl Live {
    pub fn new(now: Instant) -> Self {
        let mut nodes = HashMap::new();
        for n in NODES {
            nodes.insert(n, NodeState::default());
        }
        Self {
            nodes,
            lo: false,
            sods: false,
            soe: false,
            total_frames: 0,
            total_crc_fail: 0,
            total_bytes: 0,
            window: LinkWindow::new(now),
        }
    }

    /// A node counts as "sending" when anything of it arrived in the last 3 s
    /// (env + status both beat at 1 Hz, so 3 s tolerates two dropped frames)
    pub fn sending(&self, node: &str, now: Instant) -> bool {
        self.nodes
            .get(node)
            .and_then(|n| n.last_seen)
            .is_some_and(|t| now.duration_since(t).as_secs() < 3)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::Duration;

    #[test]
    fn window_counts_and_resets() {
        let t0 = Instant::now();
        let mut w = LinkWindow::new(t0);
        w.record(t0, true);
        w.record(t0, false);
        w.record_bytes(t0, 52);
        let (_, frames, fails, bytes) = w.summary(t0);
        assert_eq!((frames, fails, bytes), (2, 1, 52));

        // one reset clears packets, crc fails and throughput together -
        // they are three views of the same window
        w.reset(t0 + Duration::from_secs(1));
        let (_, frames, fails, bytes) = w.summary(t0 + Duration::from_secs(1));
        assert_eq!((frames, fails, bytes), (0, 0, 0));
    }

    #[test]
    fn window_forgets_after_ten_minutes() {
        let t0 = Instant::now();
        let mut w = LinkWindow::new(t0);
        w.record(t0, false);
        w.record_bytes(t0, 100);
        // 601 s later the bucket has been overwritten
        let (_, frames, fails, bytes) = w.summary(t0 + Duration::from_secs(601));
        assert_eq!((frames, fails, bytes), (0, 0, 0));
    }

    #[test]
    fn sending_is_freshness_based() {
        let t0 = Instant::now();
        let mut live = Live::new(t0);
        assert!(!live.sending("btc", t0));
        live.nodes.get_mut("btc").unwrap().last_seen = Some(t0);
        assert!(live.sending("btc", t0 + Duration::from_secs(1)));
        assert!(!live.sending("btc", t0 + Duration::from_secs(5)));
    }
}
