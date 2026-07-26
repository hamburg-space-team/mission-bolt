use serde::Serialize;

use crate::error::DecodeError;

// PayloadType (enum + from_u8 + name) is generated from bolt/wire/types.hpp's enum.
include!(concat!(env!("OUT_DIR"), "/types_gen.rs"));

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

// --- payload structs + decode -------------------------------------------
// The wire structs and their byte-offset decode() are GENERATED from schema.json
// (build.rs) - the layout lives only in the flight header, never hand-mirrored
// here. Grouping (accel x/y/z -> array) and engineering conversion happen in
// model.rs::normalize, not on these raw structs.
fn arr_u8<const N: usize>(b: &[u8], off: usize) -> [u8; N] {
    let mut a = [0u8; N];
    for (i, v) in a.iter_mut().enumerate() {
        *v = b[off + i];
    }
    a
}
fn arr_u16<const N: usize>(b: &[u8], off: usize) -> [u16; N] {
    let mut a = [0u16; N];
    for (i, v) in a.iter_mut().enumerate() {
        *v = u16le(b, off + i * 2);
    }
    a
}

// Structs BtcEnv/BtcStatus/BtcImu/Exp1Spectrum/ExpEnv/... each with `fn decode(b)`.
include!(concat!(env!("OUT_DIR"), "/payloads_gen.rs"));

// One decoded payload, still in raw register units.
#[derive(Debug, Clone, Serialize)]
#[serde(tag = "payload", rename_all = "snake_case")]
pub enum Payload {
    BtcEnv(BtcEnv),
    BtcStatus(BtcStatus),
    BtcImu(BtcImu),
    Spectrum(Exp1Spectrum),
    Exp1Env(Exp1Env),
    Exp2Env(Exp2Env),
    Exp3Env(Exp3Env),
    Exp1Status(Exp1Status),
    Exp2Status(Exp2Status),
    Exp2Ber(Exp2Ber),
    Exp3StackA(Exp3StackA),
    Exp3StackB(Exp3StackB),
    Exp3Imu(Exp3Imu),
    Exp3Status(Exp3Status),
    GapMarker(GapMarker),
    Fault(Fault),
    CmdAck(CmdAck),
    // One TIMING type per node (the header names the origin, so the payload is
    // durations only) - same shape as the per-node ENV/STATUS types
    BtcTiming(BtcTiming),
    Exp1Timing(Exp1Timing),
    Exp2Timing(Exp2Timing),
    Exp3Timing(Exp3Timing),
    // One self-test result type per node, same per-node convention as TIMING
    BtcTest(BtcTest),
    Exp1Test(Exp1Test),
    Exp2Test(Exp2Test),
    Exp3Test(Exp3Test),
    Boot(Boot),
}

// Expected on-wire payload size for a type, straight from the generated schema.
fn expected_size(ty: PayloadType) -> Option<usize> {
    crate::schema::payload(ty.schema_name()).map(|p| p.size as usize)
}

// Strict length gate: the payload must be exactly the size the schema says for
// its type. A bit-flip in the type/length byte (or a resync onto a false header)
// that lands on a different type is rejected here instead of decoding garbage.
// The exact match also guarantees every field offset below is in bounds.
fn check_size(ty_byte: u8, ty: PayloadType, b: &[u8]) -> Result<(), DecodeError> {
    match expected_size(ty) {
        Some(sz) if b.len() != sz => Err(DecodeError::SizeMismatch {
            ty: ty_byte,
            expected: sz,
            got: b.len(),
        }),
        _ => Ok(()),
    }
}

pub fn decode_payload(ty_byte: u8, b: &[u8]) -> Result<Payload, DecodeError> {
    let ty = PayloadType::from_u8(ty_byte).ok_or(DecodeError::UnknownType(ty_byte))?;
    check_size(ty_byte, ty, b)?;

    // The per-struct decode() bodies are generated from schema.json; this only
    // maps the wire type byte to its payload (bolt/wire/types.hpp knowledge).
    use PayloadType as T;
    Ok(match ty {
        T::BtcEnv => Payload::BtcEnv(BtcEnv::decode(b)),
        T::BtcStatus => Payload::BtcStatus(BtcStatus::decode(b)),
        T::BtcImu => Payload::BtcImu(BtcImu::decode(b)),
        T::Exp1Spectrum => Payload::Spectrum(Exp1Spectrum::decode(b)),
        T::Exp1Env => Payload::Exp1Env(Exp1Env::decode(b)),
        T::Exp2Env => Payload::Exp2Env(Exp2Env::decode(b)),
        T::Exp3Env => Payload::Exp3Env(Exp3Env::decode(b)),
        T::Exp3Imu => Payload::Exp3Imu(Exp3Imu::decode(b)),
        T::Exp1Status => Payload::Exp1Status(Exp1Status::decode(b)),
        T::Exp2Status => Payload::Exp2Status(Exp2Status::decode(b)),
        T::Exp2Ber => Payload::Exp2Ber(Exp2Ber::decode(b)),
        T::Exp3StackA => Payload::Exp3StackA(Exp3StackA::decode(b)),
        T::Exp3StackB => Payload::Exp3StackB(Exp3StackB::decode(b)),
        T::Exp3Status => Payload::Exp3Status(Exp3Status::decode(b)),
        T::GapMarker => Payload::GapMarker(GapMarker::decode(b)),
        T::Fault => Payload::Fault(Fault::decode(b)),
        T::CmdAck => Payload::CmdAck(CmdAck::decode(b)),
        T::BtcTiming => Payload::BtcTiming(BtcTiming::decode(b)),
        T::Exp1Timing => Payload::Exp1Timing(Exp1Timing::decode(b)),
        T::Exp2Timing => Payload::Exp2Timing(Exp2Timing::decode(b)),
        T::Exp3Timing => Payload::Exp3Timing(Exp3Timing::decode(b)),
        T::BtcTest => Payload::BtcTest(BtcTest::decode(b)),
        T::Exp1Test => Payload::Exp1Test(Exp1Test::decode(b)),
        T::Exp2Test => Payload::Exp2Test(Exp2Test::decode(b)),
        T::Exp3Test => Payload::Exp3Test(Exp3Test::decode(b)),
        T::Boot => Payload::Boot(Boot::decode(b)),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    // Every wire type must map to a schema entry, so check_size actually guards it.
    #[test]
    fn every_type_has_a_schema_size() {
        for ty in [
            0x10u8, 0x11, 0x12, 0x13, 0x14, 0x20, 0x22, 0x23, 0x24, 0x25, 0x30, 0x31, 0x32, 0x33,
            0x34, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0xF0, 0xF1, 0xF2, 0xFE,
        ] {
            let t = PayloadType::from_u8(ty).unwrap();
            assert!(
                expected_size(t).is_some(),
                "no schema size for {}",
                t.schema_name()
            );
        }
    }

    // A payload whose length does not match the schema (e.g. an old 12-byte
    // BtcEnv, or a bit-flipped type) is rejected instead of decoding garbage.
    #[test]
    fn wrong_size_is_rejected_exact_size_decodes() {
        assert!(matches!(
            decode_payload(0x10, &[0u8; 12]), // BtcEnv is 11 now
            Err(DecodeError::SizeMismatch {
                expected: 11,
                got: 12,
                ..
            })
        ));
        assert!(decode_payload(0x10, &[0u8; 11]).is_ok());
    }
}
