use serde::Serialize;

use crate::error::DecodeError;

// Wire value of the header `type` byte for downlink payloads.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[repr(u8)]
pub enum PayloadType {
    BtcEnv = 0x10,
    BtcStatus = 0x11,
    BtcImu = 0x12,
    Exp1SpectrumA = 0x20,
    Exp1SpectrumB = 0x21,
    Exp1Env = 0x22,
    Exp1Status = 0x23,
    Exp2Ber = 0x30,
    Exp2Status = 0x31,
    Exp2Env = 0x32,
    Exp3StackA = 0x40,
    Exp3StackB = 0x41,
    Exp3Env = 0x42,
    Exp3Status = 0x43,
    Exp3Imu = 0x44,
    GapMarker = 0xF0,
    Fault = 0xF1,
    CmdAck = 0xF2,
    Boot = 0xFE,
}

impl PayloadType {
    #[must_use]
    pub fn from_u8(v: u8) -> Option<PayloadType> {
        use PayloadType::*;
        Some(match v {
            0x10 => BtcEnv,
            0x11 => BtcStatus,
            0x12 => BtcImu,
            0x20 => Exp1SpectrumA,
            0x21 => Exp1SpectrumB,
            0x22 => Exp1Env,
            0x23 => Exp1Status,
            0x30 => Exp2Ber,
            0x31 => Exp2Status,
            0x32 => Exp2Env,
            0x40 => Exp3StackA,
            0x41 => Exp3StackB,
            0x42 => Exp3Env,
            0x43 => Exp3Status,
            0x44 => Exp3Imu,
            0xF0 => GapMarker,
            0xF1 => Fault,
            0xF2 => CmdAck,
            0xFE => Boot,
            _ => return None,
        })
    }

    // Short stable name used in the manifest / UI grouping.
    #[must_use]
    pub fn name(self) -> &'static str {
        use PayloadType::*;
        match self {
            BtcEnv => "btc_env",
            BtcStatus => "btc_status",
            BtcImu => "btc_imu",
            Exp1SpectrumA => "exp1_spectrum_a",
            Exp1SpectrumB => "exp1_spectrum_b",
            Exp1Env => "exp1_env",
            Exp1Status => "exp1_status",
            Exp2Ber => "exp2_ber",
            Exp2Status => "exp2_status",
            Exp2Env => "exp2_env",
            Exp3StackA => "exp3_stack_a",
            Exp3StackB => "exp3_stack_b",
            Exp3Env => "exp3_env",
            Exp3Status => "exp3_status",
            Exp3Imu => "exp3_imu",
            GapMarker => "gap_marker",
            Fault => "fault",
            CmdAck => "cmd_ack",
            Boot => "boot",
        }
    }

    // Logical experiment/source this payload belongs to.
    #[must_use]
    pub fn source(self) -> &'static str {
        use PayloadType::*;
        match self {
            BtcEnv | BtcStatus | BtcImu => "btc",
            Exp1SpectrumA | Exp1SpectrumB | Exp1Env | Exp1Status => "exp1",
            Exp2Ber | Exp2Status | Exp2Env => "exp2",
            Exp3StackA | Exp3StackB | Exp3Env | Exp3Status | Exp3Imu => "exp3",
            GapMarker | Fault | CmdAck | Boot => "system",
        }
    }
}

// --- little-endian readers -------------------------------------------------
#[inline]
fn u16le(b: &[u8], o: usize) -> u16 {
    u16::from_le_bytes([b[o], b[o + 1]])
}
#[inline]
fn u32le(b: &[u8], o: usize) -> u32 {
    u32::from_le_bytes([b[o], b[o + 1], b[o + 2], b[o + 3]])
}
#[inline]
fn i16le(b: &[u8], o: usize) -> i16 {
    i16::from_le_bytes([b[o], b[o + 1]])
}
#[inline]
fn i32le(b: &[u8], o: usize) -> i32 {
    i32::from_le_bytes([b[o], b[o + 1], b[o + 2], b[o + 3]])
}

// --- payload structs (raw register values) ---------------------------------

#[derive(Debug, Clone, Copy, Serialize)]
pub struct BtcEnv {
    pub valid_mask: u8,
    pub temp_raw: i16,
    pub ms_pressure: u32,
    pub ms_temperature: u32,
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct BtcStatus {
    pub uptime_s: u32,
    pub lo_rtc_s: u32,
    pub sd_status: u8,
    pub signal_mask: u8,
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct Imu {
    pub accel_raw: [i16; 3],
    pub gyro_raw: [i16; 3],
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct SpectrumA {
    pub channels: [u32; 9],
    pub integration_cycles: u8,
    pub gain: u8,
    pub led_mask: u8,
    pub measurement_valid: u8,
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct SpectrumB {
    pub channels: [u32; 9],
    pub start_timestamp_us: u32,
    // None for pre-measurement_valid captures (40-byte SpectrumB).
    pub measurement_valid: Option<u8>,
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct ExpEnv {
    pub valid_mask: u8,
    pub temp_raw: i16,
    pub ms_pressure: u32,
    pub ms_temperature: u32,
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct ExpStatus {
    pub uptime_s: u32,
    pub sd_status: u8,
    pub led_write_fails: u8,
    pub spec_start_fails: u8,
    pub data_ready_fails: u8,
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct Exp2Ber {
    pub rate_index: u8,
    pub timestamp_send_us: u32,
    pub timestamp_recv_us: u32,
    pub bits_sent: u16,
    pub bit_errors: u16,
    pub first_error_byte: u8,
    pub last_error_byte: u8,
    pub measurement_valid: u8,
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct Exp3StackA {
    pub mag_raw: [i32; 3],
    pub accel_raw: [i16; 3],
    pub gyro_raw: [i16; 3],
    pub tmp_raw: i16,
    pub lifi_timestamp_us: u32,
    pub latency_us: u32,
    pub burst_index: u8,
    pub valid_mask: u8,
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct Exp3StackB {
    pub mag_raw: [i32; 3],
    pub accel_raw: [i16; 3],
    pub gyro_raw: [i16; 3],
    pub tmp_raw: i16,
    pub cap_voltage: u16,
    pub lifi_timestamp_us: u32,
    pub latency_us: u32,
    pub burst_index: u8,
    pub valid_mask: u8,
}


#[derive(Debug, Clone, Copy, Serialize)]
pub struct Exp3Status {
    pub latency_a_estimated_us: u32,
    pub latency_b_estimated_us: u32,
    pub wait_a_used_us: u32,
    pub sd_status: u8,
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct GapMarker {
    pub first_missing_tick: u16,
    pub count: u8,
    pub reason: u8,
    // Origin node (NodeId); 0xFF = unknown / pre-source_node capture.
    pub source_node: u8,
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct Fault {
    pub fault_code: u8,
    pub error_code: u8,
    pub flags: u8,
    pub depth: u8,
    pub line: u16,
    pub steps: [u8; 6],
    pub source_node: u8,
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct Boot {
    pub reason: u8,
    pub reboot_count: u16,
    pub source_node: u8,
}

#[derive(Debug, Clone, Copy, Serialize)]
pub struct CmdAck {
    pub opcode: u8,
    pub seq: u8,
    pub status: u8,
}

// One decoded payload, still in raw register units.
#[derive(Debug, Clone, Serialize)]
#[serde(tag = "payload", rename_all = "snake_case")]
pub enum Payload {
    BtcEnv(BtcEnv),
    BtcStatus(BtcStatus),
    BtcImu(Imu),
    SpectrumA(SpectrumA),
    SpectrumB(SpectrumB),
    ExpEnv { source: &'static str, data: ExpEnv },
    ExpStatus { source: &'static str, data: ExpStatus },
    Exp2Ber(Exp2Ber),
    Exp3StackA(Exp3StackA),
    Exp3StackB(Exp3StackB),
    Exp3Imu(Imu),
    Exp3Status(Exp3Status),
    GapMarker(GapMarker),
    Fault(Fault),
    CmdAck(CmdAck),
    Boot(Boot),
}

fn need(ty: u8, b: &[u8], n: usize) -> Result<(), DecodeError> {
    if b.len() < n {
        Err(DecodeError::ShortPayload { ty, need: n, got: b.len() })
    } else {
        Ok(())
    }
}

fn chans9_at(b: &[u8], start: usize) -> [u32; 9] {
    let mut c = [0u32; 9];
    for (i, slot) in c.iter_mut().enumerate() {
        *slot = u32le(b, start + i * 4);
    }
    c
}

pub fn decode_payload(ty_byte: u8, b: &[u8]) -> Result<Payload, DecodeError> {
    let ty = PayloadType::from_u8(ty_byte).ok_or(DecodeError::UnknownType(ty_byte))?;

    use PayloadType as T;
    Ok(match ty {
        T::BtcEnv => {.
            need(ty_byte, b, 12)?;
            Payload::BtcEnv(BtcEnv {
                valid_mask: b[0],
                temp_raw: i16le(b, 2),
                ms_pressure: u32le(b, 4),
                ms_temperature: u32le(b, 8),
            })
        }
        T::BtcStatus => {
            need(ty_byte, b, 12)?;
            Payload::BtcStatus(BtcStatus {
                uptime_s: u32le(b, 0),
                lo_rtc_s: u32le(b, 4),
                sd_status: b[8],
                signal_mask: b[9],
            })
        }
        T::BtcImu => {
            need(ty_byte, b, 12)?;
            Payload::BtcImu(Imu {
                accel_raw: [i16le(b, 0), i16le(b, 2), i16le(b, 4)],
                gyro_raw: [i16le(b, 6), i16le(b, 8), i16le(b, 10)],
            })
        }
        T::Exp1SpectrumA => {
            need(ty_byte, b, 40)?;
            Payload::SpectrumA(SpectrumA {
                channels: chans9_at(b, 0),
                integration_cycles: b[36],
                gain: b[37],
                led_mask: b[38],
                measurement_valid: b[39],
            })
        }
        T::Exp1SpectrumB => {
            need(ty_byte, b, 40)?;
            Payload::SpectrumB(SpectrumB {
                channels: chans9_at(b, 0),
                start_timestamp_us: u32le(b, 36),
                measurement_valid: if b.len() >= 41 { Some(b[40]) } else { None },
            })
        }
        // EXP3_ENV is now the shared PayloadExpEnv too (IMU split into EXP3_IMU).
        T::Exp1Env | T::Exp2Env | T::Exp3Env => {
            need(ty_byte, b, 12)?;
            Payload::ExpEnv {
                source: ty.source(),
                data: ExpEnv {
                    valid_mask: b[0],
                    temp_raw: i16le(b, 2),
                    ms_pressure: u32le(b, 4),
                    ms_temperature: u32le(b, 8),
                },
            }
        }
        T::Exp3Imu => {
            need(ty_byte, b, 12)?;
            Payload::Exp3Imu(Imu {
                accel_raw: [i16le(b, 0), i16le(b, 2), i16le(b, 4)],
                gyro_raw: [i16le(b, 6), i16le(b, 8), i16le(b, 10)],
            })
        }
        T::Exp1Status | T::Exp2Status => {
            need(ty_byte, b, 8)?;
            Payload::ExpStatus {
                source: ty.source(),
                data: ExpStatus {
                    uptime_s: u32le(b, 0),
                    sd_status: b[4],
                    led_write_fails: b[5],
                    spec_start_fails: b[6],
                    data_ready_fails: b[7],
                },
            }
        }
        T::Exp2Ber => {
            need(ty_byte, b, 16)?;
            Payload::Exp2Ber(Exp2Ber {
                rate_index: b[0],
                timestamp_send_us: u32le(b, 1),
                timestamp_recv_us: u32le(b, 5),
                bits_sent: u16le(b, 9),
                bit_errors: u16le(b, 11),
                first_error_byte: b[13],
                last_error_byte: b[14],
                measurement_valid: b[15],
            })
        }
        T::Exp3StackA => {
            need(ty_byte, b, 39)?;
            Payload::Exp3StackA(Exp3StackA {
                mag_raw: [i32le(b, 0), i32le(b, 4), i32le(b, 8)],
                accel_raw: [i16le(b, 12), i16le(b, 14), i16le(b, 16)],
                gyro_raw: [i16le(b, 18), i16le(b, 20), i16le(b, 22)],
                tmp_raw: i16le(b, 24),
                lifi_timestamp_us: u32le(b, 26),
                latency_us: u32le(b, 30),
                burst_index: b[34],
                valid_mask: b[35],
            })
        }
        T::Exp3StackB => {
            need(ty_byte, b, 40)?;
            Payload::Exp3StackB(Exp3StackB {
                mag_raw: [i32le(b, 0), i32le(b, 4), i32le(b, 8)],
                accel_raw: [i16le(b, 12), i16le(b, 14), i16le(b, 16)],
                gyro_raw: [i16le(b, 18), i16le(b, 20), i16le(b, 22)],
                tmp_raw: i16le(b, 24),
                cap_voltage: u16le(b, 26),
                lifi_timestamp_us: u32le(b, 28),
                latency_us: u32le(b, 32),
                burst_index: b[36],
                valid_mask: b[37],
            })
        }
        T::Exp3Status => {
            need(ty_byte, b, 13)?;
            Payload::Exp3Status(Exp3Status {
                latency_a_estimated_us: u32le(b, 0),
                latency_b_estimated_us: u32le(b, 4),
                wait_a_used_us: u32le(b, 8),
                sd_status: b[12],
            })
        }
        T::GapMarker => {
            need(ty_byte, b, 4)?;
            Payload::GapMarker(GapMarker {
                first_missing_tick: u16le(b, 0),
                count: b[2],
                reason: b[3],
                source_node: if b.len() >= 5 { b[4] } else { 0xFF },
            })
        }
        T::Fault => {
            need(ty_byte, b, 12)?;
            Payload::Fault(Fault {
                fault_code: b[0],
                error_code: b[1],
                flags: b[2],
                depth: b[3],
                line: u16le(b, 4),
                steps: [b[6], b[7], b[8], b[9], b[10], b[11]],
                source_node: if b.len() >= 13 { b[12] } else { 0xFF },
            })
        }
        T::CmdAck => {
            need(ty_byte, b, 3)?;
            Payload::CmdAck(CmdAck { opcode: b[0], seq: b[1], status: b[2] })
        }
        T::Boot => {
            need(ty_byte, b, 4)?;
            Payload::Boot(Boot {
                reason: b[0],
                reboot_count: u16le(b, 2),
                source_node: if b.len() >= 5 { b[4] } else { 0xFF },
            })
        }
    })
}
