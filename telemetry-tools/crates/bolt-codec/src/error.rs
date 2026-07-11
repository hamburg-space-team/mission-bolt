use core::fmt;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DecodeError {
    // Type byte is not a known downlink payload for the selected mission.
    UnknownType(u8),
    // Payload was shorter than the fixed layout for its type.
    ShortPayload { ty: u8, need: usize, got: usize },
}

impl fmt::Display for DecodeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DecodeError::UnknownType(t) => write!(f, "unknown payload type 0x{t:02X}"),
            DecodeError::ShortPayload { ty, need, got } => {
                write!(f, "payload 0x{ty:02X} too short: need {need}, got {got}")
            }
        }
    }
}

impl std::error::Error for DecodeError {}
