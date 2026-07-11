pub const POLY: u16 = 0x1021;
pub const INIT: u16 = 0xFFFF;

// Compute CRC-16/CCITT-FALSE over data
#[must_use]
pub fn crc16(data: &[u8]) -> u16 {
    let mut crc = INIT;
    for &byte in data {
        crc ^= (byte as u16) << 8;
        for _ in 0..8 {
            crc = if crc & 0x8000 != 0 {
                (crc << 1) ^ POLY
            } else {
                crc << 1
            };
        }
    }
    crc
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ccitt_false_check_value() {
        assert_eq!(crc16(b"123456789"), 0x29B1);
    }
}
