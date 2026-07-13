use serde::Serialize;

use crate::mission::{Calibration, MissionSpec};
use crate::payloads::Payload;

const G_TO_MS2: f32 = 9.806_65;
const GAUSS_TO_UT: f32 = 100.0;

// Decoded REXUS discrete signal state
#[derive(Debug, Clone, Copy, Serialize)]
pub struct Signal {
    pub lo: bool,
    pub soe: bool,
    pub sods: bool,
}

impl Signal {
    fn from_mask(m: u8) -> Signal {
        Signal { lo: m & 0x01 != 0, soe: m & 0x02 != 0, sods: m & 0x04 != 0 }
    }
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct Sd {
    pub mounted: bool,
    pub failed: bool,
}

impl Sd {
    fn from_status(s: u8) -> Sd {
        Sd { mounted: s & 0x01 != 0, failed: s & 0x02 != 0 }
    }
}

// One normalized sample, ready for display/storage. Header context (tick,
// timestamp, source) is attached by the caller alongside this.
#[derive(Debug, Clone, Serialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum Sample {
    Env {
        source: &'static str,
        // TMP117 temperature (degC).
        #[serde(skip_serializing_if = "Option::is_none")]
        temp_c: Option<f32>,
        // MS5611 converted pressure (mbar) and temperature (degC).
        #[serde(skip_serializing_if = "Option::is_none")]
        pressure_mbar: Option<f32>,
        #[serde(skip_serializing_if = "Option::is_none")]
        ms_temp_c: Option<f32>,
        #[serde(skip_serializing_if = "Option::is_none")]
        ms_pressure_raw: Option<u32>,
        #[serde(skip_serializing_if = "Option::is_none")]
        ms_temperature_raw: Option<u32>,
        #[serde(skip_serializing_if = "Option::is_none")]
        accel_ms2: Option<[f32; 3]>,
        #[serde(skip_serializing_if = "Option::is_none")]
        gyro_dps: Option<[f32; 3]>,

        valid_mask: u8,
    },
    Imu {
        accel_ms2: [f32; 3],
        gyro_dps: [f32; 3],
        // ICM reset/not-ready sentinel: all 3 gyro axes at raw 0x8000 at once
        invalid: bool,
    },
    Spectrum {
        // All 18 AS7265x channels, gain-normalized (one atomic measurement).
        channels: [f32; 18],
        gain_index: u8,
        led_mask: u8,
        integration_cycles: u8,
        measurement_valid: bool,
        start_timestamp_us: u32,
    },
    Status {
        source: &'static str,
        uptime_s: u32,
        #[serde(skip_serializing_if = "Option::is_none")]
        lo_rtc_s: Option<u32>,
        #[serde(skip_serializing_if = "Option::is_none")]
        signal: Option<Signal>,
        sd: Sd,
        #[serde(skip_serializing_if = "Option::is_none")]
        led_write_fails: Option<u8>,
        #[serde(skip_serializing_if = "Option::is_none")]
        spec_start_fails: Option<u8>,
        #[serde(skip_serializing_if = "Option::is_none")]
        data_ready_fails: Option<u8>,
    },
    Ber {
        rate_index: u8,
        bits_sent: u16,
        bit_errors: u16,
        ber: f32,
        latency_us: u32,
        measurement_valid: bool,
    },
    Stack {
        which: char,
        mag_ut: [f32; 3],
        accel_ms2: [f32; 3],
        gyro_dps: [f32; 3],
        temp_c: f32,
        #[serde(skip_serializing_if = "Option::is_none")]
        cap_voltage_raw: Option<u16>,
        latency_us: u32,
        burst_index: u8,
        valid_mask: u8,
    },
    Exp3Status {
        latency_a_estimated_us: u32,
        latency_b_estimated_us: u32,
        wait_a_used_us: u32,
        sd: Sd,
    },
    Gap {
        first_missing_tick: u16,
        count: u8,
        reason: &'static str,
        source_node: u8,
        node: &'static str,
    },
    Fault {
        fault_code: u8,
        error_code: u8,
        truncated: bool,
        depth: u8,
        line: u16,
        steps: [u8; 6],
        source_node: u8,
        node: &'static str,
    },
    Boot {
        reason: &'static str,
        reboot_count: u16,
        source_node: u8,
        node: &'static str,
    },
    CmdAck {
        opcode: u8,
        seq: u8,
        status: u8,
        command: &'static str,
        result: &'static str,
    },
    Timing {
        source_node: u8,
        node: &'static str,
        // Worst-case us per scope since the last send (WCET / tick budget).
        tick_us: u16,
        read_us: u16,
        cfg_us: u16,
        drive_us: u16,
        send_us: u16,
        store_us: u16,
    },
}

impl Sample {
    #[must_use]
    pub fn columns(&self) -> Vec<(&'static str, f64)> {
        // Emit NaN when the sensor's valid bit is clear, so invalid samples
        // are never plotted as if they were real
        let nan_xyz = |base: &'static [&'static str; 3], v: &[f32; 3], ok: bool| {
            let g = |x: f32| if ok { x as f64 } else { f64::NAN };
            vec![(base[0], g(v[0])), (base[1], g(v[1])), (base[2], g(v[2]))]
        };
        let gate = |x: f32, ok: bool| if ok { x as f64 } else { f64::NAN };
        const A: [&str; 3] = ["accel_x", "accel_y", "accel_z"];
        const G: [&str; 3] = ["gyro_x", "gyro_y", "gyro_z"];
        const M: [&str; 3] = ["mag_x", "mag_y", "mag_z"];
        const CH: [&str; 18] = [
            "ch0", "ch1", "ch2", "ch3", "ch4", "ch5", "ch6", "ch7", "ch8", "ch9", "ch10", "ch11", "ch12", "ch13",
            "ch14", "ch15", "ch16", "ch17",
        ];
        match self {
            Sample::Env { temp_c, pressure_mbar, accel_ms2, gyro_dps, valid_mask, .. } => {
                let ms_ok = valid_mask & 0x01 != 0; // ms5611
                let tmp_ok = valid_mask & 0x02 != 0; // tmp117
                let imu_ok = valid_mask & 0x04 != 0; // icm42686
                let mut out = Vec::new();
                if let Some(t) = temp_c {
                    out.push(("temp_c", gate(*t, tmp_ok)));
                }
                if let Some(p) = pressure_mbar {
                    out.push(("pressure_mbar", gate(*p, ms_ok)));
                }
                if let Some(a) = accel_ms2 {
                    out.extend(nan_xyz(&A, a, imu_ok));
                }
                if let Some(gy) = gyro_dps {
                    out.extend(nan_xyz(&G, gy, imu_ok));
                }
                out
            }
            Sample::Imu { accel_ms2, gyro_dps, invalid } => {
                // NaN out the ICM reset sentinel so plots never show the spike;
                // raw_columns() still keeps the real bytes for lossless export.
                let ok = !*invalid;
                let mut out = nan_xyz(&A, accel_ms2, ok);
                out.extend(nan_xyz(&G, gyro_dps, ok));
                out
            }
            Sample::Spectrum { channels, measurement_valid, .. } => {
                // Only a DATA_RDY-confirmed readout is a real spectrum.
                let ok = *measurement_valid;
                CH.iter().zip(channels).map(|(n, v)| (*n, gate(*v, ok))).collect()
            }
            Sample::Status { uptime_s, .. } => vec![("uptime_s", *uptime_s as f64)],
            Sample::Ber { ber, bit_errors, latency_us, .. } => vec![
                ("ber", *ber as f64),
                ("bit_errors", *bit_errors as f64),
                ("latency_us", *latency_us as f64),
            ],
            Sample::Stack { mag_ut, accel_ms2, gyro_dps, temp_c, latency_us, valid_mask, .. } => {
                let mag_ok = valid_mask & 0x01 != 0;
                let imu_ok = valid_mask & 0x02 != 0;
                let tmp_ok = valid_mask & 0x04 != 0;
                let mut out = nan_xyz(&M, mag_ut, mag_ok);
                out.extend(nan_xyz(&A, accel_ms2, imu_ok));
                out.extend(nan_xyz(&G, gyro_dps, imu_ok));
                out.push(("temp_c", gate(*temp_c, tmp_ok)));
                out.push(("latency_us", *latency_us as f64)); // always meaningful
                out
            }
            Sample::Exp3Status { latency_a_estimated_us, latency_b_estimated_us, .. } => vec![
                ("latency_a_us", *latency_a_estimated_us as f64),
                ("latency_b_us", *latency_b_estimated_us as f64),
            ],
            Sample::Timing { tick_us, read_us, cfg_us, drive_us, send_us, store_us, .. } => vec![
                ("tick_us", *tick_us as f64),
                ("read_us", *read_us as f64),
                ("cfg_us", *cfg_us as f64),
                ("drive_us", *drive_us as f64),
                ("send_us", *send_us as f64),
                ("store_us", *store_us as f64),
            ],
            Sample::Gap { .. } | Sample::Fault { .. } | Sample::Boot { .. } | Sample::CmdAck { .. } => Vec::new(),
        }
    }

    #[must_use]
    pub fn raw_columns(&self) -> Vec<(&'static str, f64)> {
        let xyz = |b: &[&'static str; 3], v: &[f32; 3]| {
            vec![(b[0], v[0] as f64), (b[1], v[1] as f64), (b[2], v[2] as f64)]
        };
        let bit = |x: bool| if x { 1.0 } else { 0.0 };
        const A: [&str; 3] = ["accel_x", "accel_y", "accel_z"];
        const G: [&str; 3] = ["gyro_x", "gyro_y", "gyro_z"];
        const M: [&str; 3] = ["mag_x", "mag_y", "mag_z"];
        const CH: [&str; 18] = [
            "ch0", "ch1", "ch2", "ch3", "ch4", "ch5", "ch6", "ch7", "ch8", "ch9", "ch10", "ch11", "ch12", "ch13",
            "ch14", "ch15", "ch16", "ch17",
        ];
        match self {
            Sample::Env { temp_c, pressure_mbar, ms_temp_c, ms_pressure_raw, ms_temperature_raw, accel_ms2, gyro_dps, valid_mask, .. } => {
                let mut o: Vec<(&'static str, f64)> = vec![("valid_mask", *valid_mask as f64)];
                if let Some(t) = temp_c { o.push(("temp_c", *t as f64)); }
                if let Some(p) = pressure_mbar { o.push(("pressure_mbar", *p as f64)); }
                if let Some(t) = ms_temp_c { o.push(("ms_temp_c", *t as f64)); }
                if let Some(p) = ms_pressure_raw { o.push(("ms_pressure_raw", *p as f64)); }
                if let Some(t) = ms_temperature_raw { o.push(("ms_temperature_raw", *t as f64)); }
                if let Some(a) = accel_ms2 { o.extend(xyz(&A, a)); }
                if let Some(g) = gyro_dps { o.extend(xyz(&G, g)); }
                o
            }
            Sample::Imu { accel_ms2, gyro_dps, .. } => {
                let mut o = xyz(&A, accel_ms2);
                o.extend(xyz(&G, gyro_dps));
                o
            }
            Sample::Spectrum { channels, gain_index, led_mask, integration_cycles, measurement_valid, start_timestamp_us } => {
                let mut o: Vec<(&'static str, f64)> = vec![
                    ("gain_index", *gain_index as f64),
                    ("led_mask", *led_mask as f64),
                    ("integration_cycles", *integration_cycles as f64),
                    ("measurement_valid", bit(*measurement_valid)),
                ];
                for (n, v) in CH.iter().zip(channels) { o.push((n, *v as f64)); }
                o.push(("start_timestamp_us", *start_timestamp_us as f64));
                o
            }
            Sample::Status { uptime_s, lo_rtc_s, signal, sd, led_write_fails, spec_start_fails, data_ready_fails, .. } => {
                let mut o: Vec<(&'static str, f64)> = vec![
                    ("uptime_s", *uptime_s as f64),
                    ("sd_mounted", bit(sd.mounted)),
                    ("sd_failed", bit(sd.failed)),
                ];
                if let Some(l) = lo_rtc_s { o.push(("lo_rtc_s", *l as f64)); }
                if let Some(sig) = signal {
                    o.push(("lo", bit(sig.lo)));
                    o.push(("soe", bit(sig.soe)));
                    o.push(("sods", bit(sig.sods)));
                }
                if let Some(x) = led_write_fails { o.push(("led_write_fails", *x as f64)); }
                if let Some(x) = spec_start_fails { o.push(("spec_start_fails", *x as f64)); }
                if let Some(x) = data_ready_fails { o.push(("data_ready_fails", *x as f64)); }
                o
            }
            Sample::Ber { rate_index, bits_sent, bit_errors, ber, latency_us, measurement_valid } => vec![
                ("rate_index", *rate_index as f64),
                ("bits_sent", *bits_sent as f64),
                ("bit_errors", *bit_errors as f64),
                ("ber", *ber as f64),
                ("latency_us", *latency_us as f64),
                ("measurement_valid", bit(*measurement_valid)),
            ],
            Sample::Stack { mag_ut, accel_ms2, gyro_dps, temp_c, cap_voltage_raw, latency_us, burst_index, valid_mask, .. } => {
                let mut o = xyz(&M, mag_ut);
                o.extend(xyz(&A, accel_ms2));
                o.extend(xyz(&G, gyro_dps));
                o.push(("temp_c", *temp_c as f64));
                o.push(("latency_us", *latency_us as f64));
                o.push(("burst_index", *burst_index as f64));
                o.push(("valid_mask", *valid_mask as f64));
                if let Some(cv) = cap_voltage_raw { o.push(("cap_voltage_raw", *cv as f64)); }
                o
            }
            Sample::Exp3Status { latency_a_estimated_us, latency_b_estimated_us, wait_a_used_us, sd } => vec![
                ("latency_a_us", *latency_a_estimated_us as f64),
                ("latency_b_us", *latency_b_estimated_us as f64),
                ("wait_a_used_us", *wait_a_used_us as f64),
                ("sd_mounted", bit(sd.mounted)),
                ("sd_failed", bit(sd.failed)),
            ],
            Sample::Gap { first_missing_tick, count, source_node, .. } => vec![
                ("first_missing_tick", *first_missing_tick as f64),
                ("count", *count as f64),
                ("source_node", *source_node as f64),
            ],
            Sample::Fault { fault_code, error_code, truncated, depth, line, steps, source_node, .. } => {
                let mut o: Vec<(&'static str, f64)> = vec![
                    ("fault_code", *fault_code as f64),
                    ("error_code", *error_code as f64),
                    ("truncated", bit(*truncated)),
                    ("depth", *depth as f64),
                    ("line", *line as f64),
                    ("source_node", *source_node as f64),
                ];
                const STEP: [&str; 6] = ["step0", "step1", "step2", "step3", "step4", "step5"];
                for (n, v) in STEP.iter().zip(steps) { o.push((n, *v as f64)); }
                o
            }
            Sample::Boot { reboot_count, .. } => vec![("reboot_count", *reboot_count as f64)],
            Sample::CmdAck { opcode, seq, status, .. } => {
                vec![("opcode", *opcode as f64), ("seq", *seq as f64), ("status", *status as f64)]
            }
            Sample::Timing { tick_us, read_us, cfg_us, drive_us, send_us, store_us, .. } => vec![
                ("tick_us", *tick_us as f64),
                ("read_us", *read_us as f64),
                ("cfg_us", *cfg_us as f64),
                ("drive_us", *drive_us as f64),
                ("send_us", *send_us as f64),
                ("store_us", *store_us as f64),
            ],
        }
    }

    // True when a validity mask / measurement flag says a present value is
    // NOT real (independent of CRC, which the frame carries separately).
    // The UI flags these; plotting drops them.
    #[must_use]
    pub fn suspect(&self) -> bool {
        match self {
            Sample::Env { temp_c, accel_ms2, ms_pressure_raw, valid_mask, .. } => {
                (ms_pressure_raw.is_some() && valid_mask & 0x01 == 0)
                    || (temp_c.is_some() && valid_mask & 0x02 == 0)
                    || (accel_ms2.is_some() && valid_mask & 0x04 == 0)
            }
            Sample::Spectrum { measurement_valid, .. } => !*measurement_valid,
            Sample::Stack { valid_mask, .. } => valid_mask & 0x07 != 0x07,
            Sample::Imu { invalid, .. } => *invalid,
            _ => false,
        }
    }
}

// BtcEnv + the three EXP env payloads share this (identical layout, diff source).
fn env_sample(source: &'static str, valid_mask: u8, temp_raw: i16, ms_pressure_raw: u32, ms_temperature_raw: u32,
              c: &Calibration) -> Sample {
    let (pm, mt) = ms5611_opt(ms_pressure_raw, ms_temperature_raw, &c.ms5611_coeffs, valid_mask & 0x01 != 0);
    Sample::Env {
        source,
        temp_c: (valid_mask & 0x02 != 0).then(|| temp_c(temp_raw, c)),
        pressure_mbar: pm,
        ms_temp_c: mt,
        ms_pressure_raw: Some(ms_pressure_raw),
        ms_temperature_raw: Some(ms_temperature_raw),
        accel_ms2: None,
        gyro_dps: None,
        valid_mask,
    }
}

// The two EXP status payloads (EXP3 has its own PayloadExp3Status).
fn exp_status_sample(source: &'static str, uptime_s: u32, sd_status: u8, led_write_fails: u8, spec_start_fails: u8,
                     data_ready_fails: u8) -> Sample {
    Sample::Status {
        source,
        uptime_s,
        lo_rtc_s: None,
        signal: None,
        sd: Sd::from_status(sd_status),
        led_write_fails: Some(led_write_fails),
        spec_start_fails: Some(spec_start_fails),
        data_ready_fails: Some(data_ready_fails),
    }
}

// BtcImu and Exp3Imu are byte-identical; share the sample construction.
fn imu_sample(accel: [i16; 3], gyro: [i16; 3], c: &Calibration) -> Sample {
    Sample::Imu {
        accel_ms2: accel_ms2(accel, c),
        gyro_dps: gyro_dps(gyro, c),
        // All 3 gyro axes pinned to i16::MIN = the ICM "gyro not ready" sentinel
        // after a reset; a real flight never hits the exact negative rail on all
        // 3 at once, so this flags only the re-init glitch, not real motion.
        invalid: gyro == [i16::MIN; 3],
    }
}
fn accel_ms2(raw: [i16; 3], c: &Calibration) -> [f32; 3] {
    raw.map(|v| v as f32 / c.accel_lsb_per_g * G_TO_MS2)
}
fn gyro_dps(raw: [i16; 3], c: &Calibration) -> [f32; 3] {
    raw.map(|v| v as f32 / c.gyro_lsb_per_dps)
}
fn temp_c(raw: i16, c: &Calibration) -> f32 {
    raw as f32 / c.temp_lsb_per_c
}
fn mag_ut(raw: [i32; 3], c: &Calibration) -> [f32; 3] {
    raw.map(|v| (v as f32 - c.mag_zero_offset) / c.mag_lsb_per_gauss * GAUSS_TO_UT)
}
// MS5611 D1/D2 raw ADC -> (pressure mbar, temperature degC), per the
// datasheet 2nd-order algorithm, using the mission's PROM coefficients.
fn ms5611_convert(d1: u32, d2: u32, coeffs: &[u32; 6]) -> (f32, f32) {
    // i128 throughout: raw D1/D2 can be garbage on invalid frames and the
    // products (d1*sens, ...) overflow i64 there. i128 can't overflow here.
    let c1 = coeffs[0] as i128;
    let c2 = coeffs[1] as i128;
    let c3 = coeffs[2] as i128;
    let c4 = coeffs[3] as i128;
    let c5 = coeffs[4] as i128;
    let c6 = coeffs[5] as i128;
    let d1 = d1 as i128;
    let d2 = d2 as i128;

    let dt = d2 - c5 * 256;
    let mut temp = 2000 + dt * c6 / 8_388_608; // 2^23
    let mut off = c2 * 65_536 + c4 * dt / 128; // 2^16, 2^7
    let mut sens = c1 * 32_768 + c3 * dt / 256; // 2^15, 2^8

    if temp < 2000 {
        let t2 = dt * dt / 2_147_483_648; // 2^31
        let mut off2 = 5 * (temp - 2000).pow(2) / 2;
        let mut sens2 = 5 * (temp - 2000).pow(2) / 4;
        if temp < -1500 {
            off2 += 7 * (temp + 1500).pow(2);
            sens2 += 11 * (temp + 1500).pow(2) / 2;
        }
        temp -= t2;
        off -= off2;
        sens -= sens2;
    }

    let p = (d1 * sens / 2_097_152 - off) / 32_768; // 2^21, 2^15
    (p as f32 / 100.0, temp as f32 / 100.0) // 0.01 mbar, 0.01 degC
}

// Convert only when the MS5611 is valid (mask bit0) and actually reported
// data - otherwise a 0/0 raw would produce nonsense pressure/temperature.
fn ms5611_opt(d1: u32, d2: u32, coeffs: &[u32; 6], valid: bool) -> (Option<f32>, Option<f32>) {
    if valid && (d1 != 0 || d2 != 0) {
        let (p, t) = ms5611_convert(d1, d2, coeffs);
        (Some(p), Some(t))
    } else {
        (None, None)
    }
}

fn spectrum(chans: [u16; 18], gain_index: u8, c: &Calibration) -> [f32; 18] {
    let g = *c.spectrum_gain.get(gain_index as usize).unwrap_or(&1.0);
    chans.map(|v| v as f32 / g)
}

// Enum byte->name lookups, generated from schema["enums"] - the WIRE(.desc)
// annotated enums in bolt/wire/types.hpp + uplink.hpp. Provides gap_reason,
// boot_reason, node_name, command_name and ack_status_name; single source, no
// hand-mirroring of the firmware enums.
include!(concat!(env!("OUT_DIR"), "/enums_gen.rs"));

// Convert a raw [`Payload`] to a normalized [`Sample`] using the mission's
// calibration.
#[must_use]
pub fn normalize(payload: &Payload, spec: &MissionSpec) -> Sample {
    let c = &spec.calibration;
    match payload {
        Payload::BtcEnv(p) => env_sample("btc", p.valid_mask, p.temp_raw, p.ms_pressure_raw, p.ms_temperature_raw, c),
        Payload::Exp1Env(p) => env_sample("exp1", p.valid_mask, p.temp_raw, p.ms_pressure_raw, p.ms_temperature_raw, c),
        Payload::Exp2Env(p) => env_sample("exp2", p.valid_mask, p.temp_raw, p.ms_pressure_raw, p.ms_temperature_raw, c),
        Payload::Exp3Env(p) => env_sample("exp3", p.valid_mask, p.temp_raw, p.ms_pressure_raw, p.ms_temperature_raw, c),
        Payload::BtcImu(p) => imu_sample([p.accel_x_raw, p.accel_y_raw, p.accel_z_raw],
                                         [p.gyro_x_raw, p.gyro_y_raw, p.gyro_z_raw], c),
        Payload::Exp3Imu(p) => imu_sample([p.accel_x_raw, p.accel_y_raw, p.accel_z_raw],
                                          [p.gyro_x_raw, p.gyro_y_raw, p.gyro_z_raw], c),
        Payload::Spectrum(p) => Sample::Spectrum {
            channels: spectrum(p.channels, p.gain, c),
            gain_index: p.gain,
            led_mask: p.led_mask,
            integration_cycles: p.integration_cycles,
            measurement_valid: p.measurement_valid != 0,
            start_timestamp_us: p.start_timestamp_us,
        },
        Payload::BtcStatus(p) => Sample::Status {
            source: "btc",
            uptime_s: p.uptime_s,
            lo_rtc_s: Some(p.lo_rtc_s),
            signal: Some(Signal::from_mask(p.signal_mask)),
            sd: Sd::from_status(p.sd_status),
            led_write_fails: None,
            spec_start_fails: None,
            data_ready_fails: None,
        },
        Payload::Exp1Status(p) => exp_status_sample("exp1", p.uptime_s, p.sd_status, p.led_write_fails,
                                                    p.spec_start_fails, p.data_ready_fails),
        Payload::Exp2Status(p) => exp_status_sample("exp2", p.uptime_s, p.sd_status, p.led_write_fails,
                                                    p.spec_start_fails, p.data_ready_fails),
        Payload::Exp2Ber(p) => Sample::Ber {
            rate_index: p.rate_index,
            bits_sent: p.bits_sent,
            bit_errors: p.bit_errors,
            ber: if p.bits_sent == 0 { 0.0 } else { p.bit_errors as f32 / p.bits_sent as f32 },
            latency_us: p.timestamp_recv_us.wrapping_sub(p.timestamp_send_us),
            measurement_valid: p.measurement_valid != 0,
        },
        Payload::Exp3StackA(p) => Sample::Stack {
            which: 'A',
            mag_ut: mag_ut([p.mag_x_raw, p.mag_y_raw, p.mag_z_raw], c),
            accel_ms2: accel_ms2([p.accel_x_raw, p.accel_y_raw, p.accel_z_raw], c),
            gyro_dps: gyro_dps([p.gyro_x_raw, p.gyro_y_raw, p.gyro_z_raw], c),
            temp_c: temp_c(p.temp_raw, c),
            cap_voltage_raw: None,
            latency_us: p.latency_a_us,
            burst_index: p.burst_index,
            valid_mask: p.valid_mask,
        },
        Payload::Exp3StackB(p) => Sample::Stack {
            which: 'B',
            mag_ut: mag_ut([p.mag_x_raw, p.mag_y_raw, p.mag_z_raw], c),
            accel_ms2: accel_ms2([p.accel_x_raw, p.accel_y_raw, p.accel_z_raw], c),
            gyro_dps: gyro_dps([p.gyro_x_raw, p.gyro_y_raw, p.gyro_z_raw], c),
            temp_c: temp_c(p.temp_raw, c),
            cap_voltage_raw: Some(p.cap_voltage_raw),
            latency_us: p.latency_b_us,
            burst_index: p.burst_index,
            valid_mask: p.valid_mask,
        },
        Payload::Exp3Status(p) => Sample::Exp3Status {
            latency_a_estimated_us: p.latency_a_estimated_us,
            latency_b_estimated_us: p.latency_b_estimated_us,
            wait_a_used_us: p.wait_a_used_us,
            sd: Sd::from_status(p.sd_status),
        },
        Payload::GapMarker(p) => Sample::Gap {
            first_missing_tick: p.first_missing_tick,
            count: p.count,
            reason: gap_reason(p.reason),
            source_node: p.source_node,
            node: node_name(p.source_node),
        },
        Payload::Fault(p) => Sample::Fault {
            fault_code: p.fault_code,
            error_code: p.error_code,
            truncated: p.flags & 0x01 != 0,
            depth: p.depth,
            line: p.line,
            steps: p.steps,
            source_node: p.source_node,
            node: node_name(p.source_node),
        },
        Payload::Boot(p) => Sample::Boot {
            reason: boot_reason(p.reason),
            reboot_count: p.reboot_count,
            source_node: p.source_node,
            node: node_name(p.source_node),
        },
        Payload::CmdAck(p) => Sample::CmdAck {
            opcode: p.opcode,
            seq: p.seq,
            status: p.status,
            command: command_name(p.opcode),
            result: ack_status_name(p.status),
        },
        Payload::Timing(p) => Sample::Timing {
            source_node: p.source_node,
            node: node_name(p.source_node),
            tick_us: p.max_us[0],
            read_us: p.max_us[1],
            cfg_us: p.max_us[2],
            drive_us: p.max_us[3],
            send_us: p.max_us[4],
            store_us: p.max_us[5],
        },
    }
}

#[cfg(test)]
mod tests {
    use crate::mission::default_mission;
    use crate::payloads::decode_payload;
    use crate::wire::{Frame, FrameEvent, Framer};

    use super::*;

    // Real BTC_STATUS frame from the bench dump
    const STATUS: &[u8] = &[
        0xb0, 0x17, 0x01, 0x11, 0x00, 0x0a, 0x00, 0x00, 0x23, 0x3b, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x07, 0x61,
    ];

    fn first_frame(bytes: &[u8]) -> Frame {
        match Framer::new(bytes).next() {
            Some(FrameEvent::Frame(f)) => f,
            _ => panic!("no frame"),
        }
    }

    #[test]
    fn lo_signal_decoded_from_real_status() {
        let f = first_frame(STATUS);
        assert!(f.crc_ok);
        let payload = decode_payload(f.header.ty, &f.payload).unwrap();
        let sample = normalize(&payload, default_mission());
        let Sample::Status { signal: Some(sig), lo_rtc_s: Some(lo), .. } = sample else {
            panic!("expected BTC status");
        };
        assert!(!sig.lo, "LO must be false in the bench capture");
        assert!(sig.soe && sig.sods, "SOE + SODS active (mask 0x06)");
        assert_eq!(lo, 0);
    }

    #[test]
    fn imu_gyro_sentinel_is_suspect_but_real_motion_is_not() {
        use crate::payloads::BtcImu;
        let spec = default_mission();
        let imu = |accel: [i16; 3], gyro: [i16; 3]| {
            Payload::BtcImu(BtcImu {
                accel_x_raw: accel[0], accel_y_raw: accel[1], accel_z_raw: accel[2],
                gyro_x_raw: gyro[0], gyro_y_raw: gyro[1], gyro_z_raw: gyro[2],
            })
        };
        // ICM reset sentinel: all 3 gyro axes at 0x8000 -> invalid.
        assert!(normalize(&imu([100, 0, -1000], [i16::MIN; 3]), spec).suspect());
        // Real flight: one axis at the rail, others normal -> NOT flagged.
        assert!(!normalize(&imu([0, 0, -16384], [i16::MIN, 200, -50]), spec).suspect());
        // Positive saturation on all axes -> NOT the sentinel, NOT flagged.
        assert!(!normalize(&imu([0, 0, 0], [i16::MAX; 3]), spec).suspect());
    }
}
