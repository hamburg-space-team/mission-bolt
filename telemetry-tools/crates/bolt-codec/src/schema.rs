//! Payload wire schema: the single source of truth for engineering units and
//! raw->value calibration, generated at build time from the flight header's
//! `WIRE(...)` annotations (see `interfaces/tools/schemagen`). Ground code
//! reads scales/units/gates from here instead of hardcoding them, so they can
//! never drift from the firmware struct definitions.

// One wire field of a payload.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct FieldSchema {
    // C++ member name (e.g. "accel_x_raw").
    pub name: &'static str,
    // C type as written in the struct (e.g. "int16_t").
    pub ctype: &'static str,
    // Byte offset of the field within the packed payload.
    pub byte_offset: u16,
    // Element count (1 for scalars, >1 for arrays like the spectrum channels).
    pub count: u16,
    // Engineering unit ("degC", "g", "dps", "us", "raw", ...).
    pub unit: &'static str,
    // engineering = raw * scale + offset.
    pub scale: f64,
    pub offset: f64,
    // Validity source: "" always valid, "valid_mask:1" bit 1, or a whole flag
    // field name like "measurement_valid".
    pub gate: &'static str,
    // Short human label for the UI.
    pub desc: &'static str,
}

impl FieldSchema {
    // Convert a raw register value to its engineering value.
    #[must_use]
    pub fn to_engineering(&self, raw: f64) -> f64 {
        raw * self.scale + self.offset
    }

    // The (field, bit) that gates this field, if it is `valid_mask:N` style.
    #[must_use]
    pub fn gate_bit(&self) -> Option<(&'static str, u8)> {
        let (field, bit) = self.gate.split_once(':')?;
        Some((field, bit.parse().ok()?))
    }
}

// One payload variant and its fields.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct PayloadSchema {
    // C++ struct name (e.g. "PayloadBtcImu").
    pub name: &'static str,
    // Total packed size in bytes.
    pub size: u16,
    pub fields: &'static [FieldSchema],
}

impl PayloadSchema {
    // Find a field by its C++ member name.
    #[must_use]
    pub fn field(&self, name: &str) -> Option<&'static FieldSchema> {
        self.fields.iter().find(|f| f.name == name)
    }
}

// pub static SCHEMA: &[PayloadSchema] = &[ ... ];
include!(concat!(env!("OUT_DIR"), "/schema_gen.rs"));

// Look up a payload's schema by its C++ struct name.
#[must_use]
pub fn payload(name: &str) -> Option<&'static PayloadSchema> {
    SCHEMA.iter().find(|p| p.name == name)
}

// All known payload schemas.
#[must_use]
pub fn all() -> &'static [PayloadSchema] {
    SCHEMA
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn imu_scales_come_from_the_flight_header() {
        let imu = payload("PayloadBtcImu").expect("BtcImu in schema");
        let ax = imu.field("accel_x_raw").unwrap();
        assert_eq!(ax.unit, "g");
        // +-32 g over 16 bit.
        assert!((ax.scale - 32.0 / 32768.0).abs() < 1e-12);
        // raw 16384 -> +16 g.
        assert!((ax.to_engineering(16384.0) - 16.0).abs() < 1e-9);

        let gz = imu.field("gyro_z_raw").unwrap();
        assert_eq!(gz.unit, "dps");
        assert!((gz.scale - 2000.0 / 32768.0).abs() < 1e-12);
    }

    #[test]
    fn gates_and_offsets_are_parsed() {
        let env = payload("PayloadBtcEnv").expect("BtcEnv in schema");
        let temp = env.field("temp_raw").unwrap();
        assert_eq!(temp.unit, "degC");
        assert!((temp.to_engineering(128.0) - 1.0).abs() < 1e-9); // 1/128 degC per LSB
        assert_eq!(temp.gate_bit(), Some(("valid_mask", 1)));
        assert_eq!(temp.byte_offset, 1); // right after valid_mask (reserved removed)
    }

    #[test]
    fn all_payloads_present() {
        assert!(all().len() >= 16);
        // One TIMING layout per node (the type byte names the origin)
        for p in ["PayloadBtcTiming", "PayloadExp1Timing", "PayloadExp2Timing", "PayloadExp3Timing"] {
            assert!(payload(p).is_some(), "missing schema entry for {p}");
        }
    }
}
