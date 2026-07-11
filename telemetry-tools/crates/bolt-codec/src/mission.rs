#[derive(Debug, Clone, Copy)]
pub struct Calibration {
    // ICM accelerometer LSB per g (32768 / full-scale-g). Bench dump was
    // +/-32 g -> 1024 LSB/g.
    pub accel_lsb_per_g: f32,
    // ICM gyroscope LSB per dps (32768 / full-scale-dps). +/-2000 dps ->
    // 16.384 LSB/dps.
    pub gyro_lsb_per_dps: f32,

    // TMP117 LSB per degC (fixed 128).
    pub temp_lsb_per_c: f32,

    pub mag_lsb_per_gauss: f32,
    pub mag_zero_offset: f32,

    // AS7265x gain-index -> multiplier (0=1x, 1=3.7x, 2=16x, 3=64x).
    pub spectrum_gain: [f32; 4],

    pub ms5611_coeffs: [u32; 6],
}

impl Calibration {
    const REXUS37: Calibration = Calibration {
        accel_lsb_per_g: 1024.0,
        gyro_lsb_per_dps: 16.384,
        temp_lsb_per_c: 128.0,
        mag_lsb_per_gauss: 16384.0,
        mag_zero_offset: 131_072.0,
        spectrum_gain: [1.0, 3.7, 16.0, 64.0],
        // MS5611 datasheet nominal example coefficients (C1..C6).
        ms5611_coeffs: [40127, 36924, 23317, 23282, 33464, 28312],
    };
}

// A mission's protocol + calibration profile.
#[derive(Debug, Clone, Copy)]
pub struct MissionSpec {
    pub id: &'static str,
    pub display_name: &'static str,
    pub protocol_version: u8,
    pub calibration: Calibration,
}

const REGISTRY: &[MissionSpec] = &[MissionSpec {
    id: "bolt",
    display_name: "Bolt (REXUS/BEXUS)",
    protocol_version: crate::wire::PROTOCOL_VERSION,
    calibration: Calibration::REXUS37,
}];

#[must_use]
pub fn registry() -> &'static [MissionSpec] {
    REGISTRY
}

#[must_use]
pub fn default_mission() -> &'static MissionSpec {
    &REGISTRY[0]
}

#[must_use]
pub fn find(id: &str) -> Option<&'static MissionSpec> {
    REGISTRY.iter().find(|m| m.id == id)
}
