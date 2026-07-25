use std::collections::BTreeMap;

use bolt_codec::{frame_source, Payload, PayloadType, Sample};
use serde::Serialize;

// One timeline anchor: a timestamp and the row index reached in each
// source's dataset - lets the UI scrub by time without scanning HDF5.
#[derive(Serialize, Default, Clone)]
pub struct TimeAnchor {
    pub t_us: u32,
    pub rows: BTreeMap<String, u64>,
}

#[derive(Serialize)]
pub struct CrcFail {
    pub index: u64,
    pub ty: u8,
    pub tick: u16,
}

#[derive(Serialize)]
pub struct GapEntry {
    pub first_missing_tick: u16,
    pub count: u8,
    pub reason: String,
    // Origin node (btc/exp1/exp2/exp3/unknown).
    pub node: String,
}

#[derive(Serialize)]
pub struct BootEntry {
    pub reason: String,
    pub reboot_count: u16,
    pub node: String,
    pub timestamp_us: u32,
}

#[derive(Serialize)]
pub struct FaultEntry {
    pub fault_code: u8,
    pub error_code: u8,
    pub line: u16,
    pub depth: u8,
    pub truncated: bool,
    pub steps: [u8; 6],
    pub timestamp_us: u32,
    // Origin node (btc/exp1/exp2/exp3/unknown).
    pub node: String,
}

#[derive(Serialize)]
pub struct TestEntry {
    pub node: String,
    pub test_id: u8,
    pub result: u8,
    pub result_name: String,
    // raw diagnostic, judged on ground
    pub data: u32,
    // true on the node's last step
    pub last: bool,
    pub timestamp_us: u32,
}

#[derive(Serialize)]
pub struct Manifest {
    pub mission: String,
    pub protocol_version: u8,
    pub codec_version: String,
    pub packet_counts: BTreeMap<String, u64>,
    pub experiments: Vec<String>,
    pub total_frames: u64,
    pub crc_fail_total: u64,
    pub crc_fails: Vec<CrcFail>,
    pub resync_bytes: u64,
    pub gaps: Vec<GapEntry>,
    pub boots: Vec<BootEntry>,
    pub faults: Vec<FaultEntry>,
    pub tests: Vec<TestEntry>,
    // RTC second at which LO was first observed; 0 = LO never received.
    pub lo_rtc_s: u32,
    pub t_start_us: u32,
    pub t_end_us: u32,
    pub timeline: Vec<TimeAnchor>,
}

// Accumulates a manifest across a stream of frames.
pub struct Builder {
    mission: String,
    protocol_version: u8,
    packet_counts: BTreeMap<String, u64>,
    sources: std::collections::BTreeSet<String>,
    rows: BTreeMap<String, u64>,
    total: u64,
    crc_fail_total: u64,
    crc_fails: Vec<CrcFail>,
    resync_bytes: u64,
    gaps: Vec<GapEntry>,
    boots: Vec<BootEntry>,
    faults: Vec<FaultEntry>,
    tests: Vec<TestEntry>,
    lo_rtc_s: u32,
    t_start: Option<u32>,
    t_end: u32,
    timeline: Vec<TimeAnchor>,
    last_anchor_us: u32,
}

// Cap the crc-fail list so a badly corrupted capture can't bloat the
// manifest; the total count is always exact.
const MAX_CRC_LIST: usize = 4096;
// One timeline anchor at most this often (microseconds) -> ~1 Hz.
const ANCHOR_INTERVAL_US: u32 = 1_000_000;

impl Builder {
    #[must_use]
    pub fn new(mission: &str, protocol_version: u8) -> Self {
        Builder {
            mission: mission.to_string(),
            protocol_version,
            packet_counts: BTreeMap::new(),
            sources: std::collections::BTreeSet::new(),
            rows: BTreeMap::new(),
            total: 0,
            crc_fail_total: 0,
            crc_fails: Vec::new(),
            resync_bytes: 0,
            gaps: Vec::new(),
            boots: Vec::new(),
            faults: Vec::new(),
            tests: Vec::new(),
            lo_rtc_s: 0,
            t_start: None,
            t_end: 0,
            timeline: Vec::new(),
            last_anchor_us: 0,
        }
    }

    pub fn add_resync(&mut self, skipped: usize) {
        self.resync_bytes += skipped as u64;
    }

    // Record one framed packet plus its decoded payload/sample.
    pub fn add_frame(
        &mut self,
        ty: u8,
        tick: u16,
        timestamp_us: u32,
        crc_ok: bool,
        payload: &Payload,
        sample: &Sample,
    ) {
        self.total += 1;
        if !crc_ok {
            self.crc_fail_total += 1;
            if self.crc_fails.len() < MAX_CRC_LIST {
                self.crc_fails.push(CrcFail { index: self.total - 1, ty, tick });
            }
        }

        let name = PayloadType::from_u8(ty).map_or("unknown", PayloadType::name);
        // frame_source, not PayloadType::source(): TIMING/FAULT/BOOT/GAP_MARKER
        // are type-annotated SYSTEM but carry their real origin in the payload
        let source = frame_source(ty, sample);
        *self.packet_counts.entry(name.to_string()).or_default() += 1;

        // "system" is a placeholder, not a node - only real nodes are listed
        if source != "system" {
            self.sources.insert(source.to_string());
        }

        // Row index only advances for sources that get an HDF5 dataset.
        if !sample.columns().is_empty() {
            *self.rows.entry(source.to_string()).or_default() += 1;
        }

        // BOOT/FAULT carry non-sample-time headers; skip them for timeline
        // bounds (their ts is 0 / error-origin).
        let is_event = matches!(payload, Payload::Boot(_) | Payload::Fault(_));
        if !is_event {
            self.t_start.get_or_insert(timestamp_us);
            self.t_end = self.t_end.max(timestamp_us);
            if timestamp_us >= self.last_anchor_us + ANCHOR_INTERVAL_US || self.timeline.is_empty() {
                self.timeline.push(TimeAnchor { t_us: timestamp_us, rows: self.rows.clone() });
                self.last_anchor_us = timestamp_us;
            }
        }

        match (payload, sample) {
            (_, Sample::Status { lo_rtc_s: Some(lo), .. }) if *lo != 0 && self.lo_rtc_s == 0 => {
                self.lo_rtc_s = *lo;
            }
            (Payload::GapMarker(_), Sample::Gap { first_missing_tick, count, reason, node, .. }) => {
                self.gaps.push(GapEntry {
                    first_missing_tick: *first_missing_tick,
                    count: *count,
                    reason: (*reason).to_string(),
                    node: (*node).to_string(),
                });
            }
            (Payload::Boot(_), Sample::Boot { reason, reboot_count, node, .. }) => {
                self.boots.push(BootEntry {
                    reason: (*reason).to_string(),
                    reboot_count: *reboot_count,
                    node: (*node).to_string(),
                    timestamp_us,
                });
            }
            (Payload::Fault(_), Sample::Fault { fault_code, error_code, line, depth, truncated, steps, node, .. }) => {
                self.faults.push(FaultEntry {
                    fault_code: *fault_code,
                    error_code: *error_code,
                    line: *line,
                    depth: *depth,
                    truncated: *truncated,
                    steps: *steps,
                    timestamp_us,
                    node: (*node).to_string(),
                });
            }
            (_, Sample::Test { node, test_id, result, result_name, last, data }) => {
                self.tests.push(TestEntry {
                    node: (*node).to_string(),
                    test_id: *test_id,
                    result: *result,
                    result_name: (*result_name).to_string(),
                    data: *data,
                    last: *last,
                    timestamp_us,
                });
            }
            _ => {}
        }
    }

    #[must_use]
    pub fn finish(self) -> Manifest {
        Manifest {
            mission: self.mission,
            protocol_version: self.protocol_version,
            codec_version: env!("CARGO_PKG_VERSION").to_string(),
            packet_counts: self.packet_counts,
            experiments: self.sources.into_iter().collect(),
            total_frames: self.total,
            crc_fail_total: self.crc_fail_total,
            crc_fails: self.crc_fails,
            resync_bytes: self.resync_bytes,
            gaps: self.gaps,
            boots: self.boots,
            faults: self.faults,
            tests: self.tests,
            lo_rtc_s: self.lo_rtc_s,
            t_start_us: self.t_start.unwrap_or(0),
            t_end_us: self.t_end,
            timeline: self.timeline,
        }
    }
}
