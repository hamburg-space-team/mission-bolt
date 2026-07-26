use heapless::Vec as HVec;
use serde::Serialize;

use crate::crc::crc16;

pub const SYNC_0: u8 = 0xB0;
pub const SYNC_1: u8 = 0x17;
pub const PROTOCOL_VERSION: u8 = 0x01;
pub const HEADER_SIZE: usize = 12;
pub const CRC_SIZE: usize = 2;
pub const MAX_PACKET_SIZE: usize = 64;
pub const MAX_PAYLOAD: usize = MAX_PACKET_SIZE - HEADER_SIZE - CRC_SIZE; // 50

// 12-byte packet header, decoded from the wire.
#[derive(Debug, Clone, Copy, Serialize)]
pub struct Header {
    pub version: u8,
    pub ty: u8, // Type
    pub sequence: u8,
    pub length: u8,
    pub tick: u16,
    pub timestamp_us: u32,
}

impl Header {
    fn parse(buf: &[u8]) -> Header {
        Header {
            version: buf[2],
            ty: buf[3],
            sequence: buf[4],
            length: buf[5],
            tick: u16::from_le_bytes([buf[6], buf[7]]),
            timestamp_us: u32::from_le_bytes([buf[8], buf[9], buf[10], buf[11]]),
        }
    }
}

// One decoded frame. `crc_ok == false` means the sync/version/length
// framed cleanly but the CRC did not match
#[derive(Debug, Clone, Serialize)]
pub struct Frame {
    pub header: Header,
    #[serde(skip)]
    pub payload: HVec<u8, MAX_PAYLOAD>,
    pub crc_ok: bool,
}

// Events yielded while framing a byte stream.
#[derive(Debug, Clone, Serialize)]
#[serde(tag = "event", rename_all = "snake_case")]
pub enum FrameEvent {
    Frame(Frame),
    Resync { skipped: usize },
}

enum Parsed {
    // Not enough bytes yet - wait for more.
    NeedMore,
    // Advance `skip` bytes and try again (no valid frame at offset 0).
    Resync { skip: usize },
    Frame { consumed: usize, frame: Frame },
}

// Offset of the next plausible sync start at position >= 1, or the whole
// length if none - so a resync always makes forward progress.
fn next_sync(buf: &[u8]) -> usize {
    buf.iter()
        .skip(1)
        .position(|&b| b == SYNC_0)
        .map_or(buf.len(), |p| p + 1)
}

fn parse_one(buf: &[u8]) -> Parsed {
    if buf.len() < 2 {
        return Parsed::NeedMore;
    }
    if buf[0] != SYNC_0 || buf[1] != SYNC_1 {
        return Parsed::Resync {
            skip: next_sync(buf).max(1),
        };
    }
    if buf.len() < HEADER_SIZE {
        return Parsed::NeedMore;
    }
    if buf[2] != PROTOCOL_VERSION {
        return Parsed::Resync {
            skip: next_sync(buf).max(1),
        };
    }
    let len = buf[5] as usize;
    if len > MAX_PAYLOAD {
        return Parsed::Resync {
            skip: next_sync(buf).max(1),
        };
    }
    let total = HEADER_SIZE + len + CRC_SIZE;
    if buf.len() < total {
        return Parsed::NeedMore;
    }

    let crc_calc = crc16(&buf[2..HEADER_SIZE + len]);
    let crc_wire = u16::from_be_bytes([buf[HEADER_SIZE + len], buf[HEADER_SIZE + len + 1]]);

    let header = Header::parse(buf);
    let mut payload: HVec<u8, MAX_PAYLOAD> = HVec::new();
    // Infallible: len <= MAX_PAYLOAD checked above.
    let _ = payload.extend_from_slice(&buf[HEADER_SIZE..HEADER_SIZE + len]);

    Parsed::Frame {
        consumed: total,
        frame: Frame {
            header,
            payload,
            crc_ok: crc_calc == crc_wire,
        },
    }
}

// Zero-copy framer over an in-memory slice (post-flight file path).
pub struct Framer<'a> {
    buf: &'a [u8],
    pos: usize,
    // Total bytes skipped during resyncs so far.
    pub resync_bytes: usize,
}

impl<'a> Framer<'a> {
    #[must_use]
    pub fn new(buf: &'a [u8]) -> Self {
        Framer {
            buf,
            pos: 0,
            resync_bytes: 0,
        }
    }
}

impl Iterator for Framer<'_> {
    type Item = FrameEvent;

    fn next(&mut self) -> Option<FrameEvent> {
        let rem = &self.buf[self.pos..];
        if rem.is_empty() {
            return None;
        }
        match parse_one(rem) {
            // Incomplete trailing bytes at end of file: stop.
            Parsed::NeedMore => None,
            Parsed::Resync { skip } => {
                self.pos += skip;
                self.resync_bytes += skip;
                Some(FrameEvent::Resync { skipped: skip })
            }
            Parsed::Frame { consumed, frame } => {
                self.pos += consumed;
                Some(FrameEvent::Frame(frame))
            }
        }
    }
}

#[derive(Default)]
pub struct StreamDecoder {
    buf: alloc_vec::Vec<u8>,
}

mod alloc_vec {
    pub use std::vec::Vec;
}

impl StreamDecoder {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    pub fn push(&mut self, data: &[u8]) {
        self.buf.extend_from_slice(data);
    }

    // Pull the next event, or `None` if more bytes are needed.
    pub fn pull(&mut self) -> Option<FrameEvent> {
        match parse_one(&self.buf) {
            Parsed::NeedMore => None,
            Parsed::Resync { skip } => {
                self.buf.drain(0..skip);
                Some(FrameEvent::Resync { skipped: skip })
            }
            Parsed::Frame { consumed, frame } => {
                self.buf.drain(0..consumed);
                Some(FrameEvent::Frame(frame))
            }
        }
    }

    // Bytes currently buffered but not yet framed.
    #[must_use]
    pub fn pending(&self) -> usize {
        self.buf.len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // Real BTC_IMU frame captured on the bench (from the mission-bolt downlink dump)
    const IMU: &[u8] = &[
        0xb0, 0x17, 0x01, 0x12, 0x00, 0x0c, 0x01, 0x00, 0xdb, 0x32, 0x01, 0x00, 0x08, 0x00, 0xf1,
        0xff, 0xfd, 0xfb, 0xe6, 0xff, 0x10, 0x00, 0x0c, 0x00, 0xea, 0x46,
    ];

    // Real BOOT frame, CRC 0xA565.
    const BOOT: &[u8] = &[
        0xb0, 0x17, 0x01, 0xfe, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0xa5, 0x65,
    ];

    #[test]
    fn frames_real_imu_packet() {
        let mut f = Framer::new(IMU);
        let Some(FrameEvent::Frame(fr)) = f.next() else {
            panic!("expected a frame");
        };
        assert!(fr.crc_ok, "real capture must validate");
        assert_eq!(fr.header.ty, 0x12);
        assert_eq!(fr.header.tick, 1);
        assert_eq!(fr.header.timestamp_us, 0x0001_32db);
        assert_eq!(fr.payload.len(), 12);
        assert!(f.next().is_none());
    }

    #[test]
    fn crc_matches_wire_big_endian() {
        // CRC is over everything after the two sync bytes, big-endian.
        assert_eq!(crc16(&IMU[2..IMU.len() - 2]), 0xEA46);
        assert_eq!(crc16(&BOOT[2..BOOT.len() - 2]), 0xA565);
    }

    #[test]
    fn resync_after_garbage() {
        let mut stream: Vec<u8> = vec![0xDE, 0xAD, 0xBE, 0xEF];
        stream.extend_from_slice(IMU);
        let events: Vec<_> = Framer::new(&stream).collect();
        // At least one resync, then the good frame.
        assert!(events
            .iter()
            .any(|e| matches!(e, FrameEvent::Resync { .. })));
        let frame = events.iter().find_map(|e| match e {
            FrameEvent::Frame(fr) => Some(fr),
            _ => None,
        });
        assert!(frame.unwrap().crc_ok);
    }

    #[test]
    fn stream_decoder_handles_split_boundaries() {
        let mut dec = StreamDecoder::new();
        // Feed the IMU frame one byte at a time; it must surface exactly once.
        let mut frames = 0;
        for chunk in IMU.chunks(1) {
            dec.push(chunk);
            while let Some(ev) = dec.pull() {
                if let FrameEvent::Frame(fr) = ev {
                    assert!(fr.crc_ok);
                    frames += 1;
                }
            }
        }
        assert_eq!(frames, 1);
    }

    #[test]
    fn bit_flip_is_detected_not_dropped() {
        let mut corrupt = IMU.to_vec();
        corrupt[14] ^= 0x01; // flip a payload bit
        let Some(FrameEvent::Frame(fr)) = Framer::new(&corrupt).next() else {
            panic!("frame still framed");
        };
        assert!(!fr.crc_ok, "corrupted frame must be flagged");
    }
}
