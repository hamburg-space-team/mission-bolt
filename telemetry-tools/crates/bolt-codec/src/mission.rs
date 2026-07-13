use std::sync::OnceLock;

use crate::schema;

#[derive(Debug, Clone, Copy)]
pub struct Calibration {
    // Fixed raw->engineering scales. Derived from the WIRE(...) annotations via
    // the schema (see Calibration::bolt), so the flight header is the single
    // source and these can never drift from the firmware struct.
    pub accel_lsb_per_g: f32,   // 32768 / full-scale-g   (+/-32 g -> 1024)
    pub gyro_lsb_per_dps: f32,  // 32768 / full-scale-dps (+/-2000 dps -> 16.384)
    pub temp_lsb_per_c: f32,    // TMP117 fixed 128
    pub mag_lsb_per_gauss: f32, // MMC5983MA 16384 (18-bit)

    // Genuinely calibratable / runtime - NOT in the schema.
    pub mag_zero_offset: f32,
    // AS7265x gain-index -> multiplier (0=1x, 1=3.7x, 2=16x, 3=64x).
    pub spectrum_gain: [f32; 4],
    pub ms5611_coeffs: [u32; 6],
}

// 1 / schema scale = LSB per engineering unit for a linear field.
fn lsb_per_unit(payload: &str, field: &str) -> f32 {
    let scale = schema::payload(payload).and_then(|p| p.field(field)).map_or(1.0, |f| f.scale);
    (1.0 / scale) as f32
}

impl Calibration {
    // The scales come from the flight header's WIRE annotations; only the
    // bias / gain table / PROM coefficients are set here.
    fn bolt() -> Calibration {
        Calibration {
            accel_lsb_per_g: lsb_per_unit("PayloadBtcImu", "accel_x_raw"),
            gyro_lsb_per_dps: lsb_per_unit("PayloadBtcImu", "gyro_x_raw"),
            temp_lsb_per_c: lsb_per_unit("PayloadBtcEnv", "temp_raw"),
            mag_lsb_per_gauss: lsb_per_unit("PayloadExp3StackA", "mag_x_raw"),
            mag_zero_offset: 131_072.0,
            spectrum_gain: [1.0, 3.7, 16.0, 64.0],
            // MS5611 datasheet nominal example coefficients (C1..C6).
            ms5611_coeffs: [40127, 36924, 23317, 23282, 33464, 28312],
        }
    }
}

// A mission's protocol + calibration profile.
#[derive(Debug, Clone, Copy)]
pub struct MissionSpec {
    pub id: &'static str,
    pub display_name: &'static str,
    pub protocol_version: u8,
    pub calibration: Calibration,
}

// Built once on first use so the schema-derived calibration is available; still
// handed out as &'static.
fn registry_impl() -> &'static [MissionSpec] {
    static REG: OnceLock<Vec<MissionSpec>> = OnceLock::new();
    REG.get_or_init(|| {
        vec![MissionSpec {
            id: "bolt",
            display_name: "Bolt (REXUS/BEXUS)",
            protocol_version: crate::wire::PROTOCOL_VERSION,
            calibration: Calibration::bolt(),
        }]
    })
}

#[must_use]
pub fn registry() -> &'static [MissionSpec] {
    registry_impl()
}

#[must_use]
pub fn default_mission() -> &'static MissionSpec {
    &registry_impl()[0]
}

#[must_use]
pub fn find(id: &str) -> Option<&'static MissionSpec> {
    registry_impl().iter().find(|m| m.id == id)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn scales_are_derived_from_the_schema() {
        let c = default_mission().calibration;
        // +-32 g / +-2000 dps / 1-128 degC / 18-bit mag, straight from the annotations.
        assert_eq!(c.accel_lsb_per_g, 1024.0);
        assert_eq!(c.gyro_lsb_per_dps, 16.384);
        assert_eq!(c.temp_lsb_per_c, 128.0);
        assert_eq!(c.mag_lsb_per_gauss, 16384.0);
    }
}
