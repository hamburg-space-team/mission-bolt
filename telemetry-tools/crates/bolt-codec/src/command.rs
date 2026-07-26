use heapless::Vec as HVec;
use serde::{Deserialize, Serialize};

use crate::crc::crc16;
use crate::wire::{HEADER_SIZE, MAX_PACKET_SIZE, MAX_PAYLOAD, PROTOCOL_VERSION, SYNC_0, SYNC_1};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u8)]
pub enum CommandType {
    ResetTick = 0x01,
    StartExperiment = 0x02,
    ActivateCamera = 0x03,
    StopExperiment = 0x04,
    FullSystemTest = 0x05,
}

// A command to send
#[derive(Debug, Clone)]
pub enum Command {
    ResetTick,
    StartExperiment,
    ActivateCamera,
    StopExperiment,
    FullSystemTest,
    Raw {
        opcode: u8,
        payload: HVec<u8, MAX_PAYLOAD>,
    },
}

impl Command {
    #[must_use]
    pub fn opcode(&self) -> u8 {
        match self {
            Command::ResetTick => CommandType::ResetTick as u8,
            Command::StartExperiment => CommandType::StartExperiment as u8,
            Command::ActivateCamera => CommandType::ActivateCamera as u8,
            Command::StopExperiment => CommandType::StopExperiment as u8,
            Command::FullSystemTest => CommandType::FullSystemTest as u8,
            Command::Raw { opcode, .. } => *opcode,
        }
    }

    #[must_use]
    pub fn payload(&self) -> &[u8] {
        match self {
            Command::Raw { payload, .. } => payload,
            _ => &[],
        }
    }

    // Parse the stable command name used by the extension UI / JSON.
    #[must_use]
    pub fn from_name(name: &str) -> Option<Command> {
        Some(match name {
            "reset_tick" => Command::ResetTick,
            "start_experiment" => Command::StartExperiment,
            "activate_camera" => Command::ActivateCamera,
            "stop_experiment" => Command::StopExperiment,
            "full_system_test" => Command::FullSystemTest,
            _ => return None,
        })
    }

    #[must_use]
    pub fn is_dangerous(&self) -> bool {
        // Single source: the `.danger` annotation on CommandOpcode (schema.json),
        // generated into command_danger() - no hardcoded list here.
        crate::model::command_danger(self.opcode())
    }
}

// Encode a command into wire bytes
#[must_use]
pub fn encode(cmd: &Command, seq: &mut u8) -> HVec<u8, MAX_PACKET_SIZE> {
    let payload = cmd.payload();
    let len = payload.len().min(MAX_PAYLOAD);

    let mut buf: HVec<u8, MAX_PACKET_SIZE> = HVec::new();
    let _ = buf.extend_from_slice(&[
        SYNC_0,
        SYNC_1,
        PROTOCOL_VERSION,
        cmd.opcode(),
        *seq,
        len as u8,
        0,
        0, // tick = 0
        0,
        0,
        0,
        0, // timestamp_us = 0
    ]);
    let _ = buf.extend_from_slice(&payload[..len]);

    let crc = crc16(&buf[2..HEADER_SIZE + len]);
    let _ = buf.push((crc >> 8) as u8); // big-endian, matches downlink
    let _ = buf.push(crc as u8);

    *seq = seq.wrapping_add(1);
    buf
}

#[cfg(test)]
mod tests {
    use crate::wire::{FrameEvent, Framer};

    use super::*;

    #[test]
    fn command_round_trips_through_the_framer() {
        let mut seq = 7u8;
        let bytes = encode(&Command::ResetTick, &mut seq);

        assert_eq!(seq, 8, "sequence advanced");

        let Some(FrameEvent::Frame(f)) = Framer::new(&bytes).next() else {
            panic!("command did not frame");
        };
        assert!(f.crc_ok, "self-encoded command must pass its own CRC");
        assert_eq!(f.header.ty, CommandType::ResetTick as u8);
        assert_eq!(f.header.sequence, 7);
        assert_eq!(f.header.length, 0);
        assert!(
            f.header.ty < 0x10,
            "uplink opcode stays in the command range"
        );
    }

    #[test]
    fn dangerous_commands_flagged() {
        assert!(Command::ResetTick.is_dangerous());
        assert!(Command::FullSystemTest.is_dangerous());
        assert!(Command::StopExperiment.is_dangerous());
        assert!(!Command::ActivateCamera.is_dangerous());
    }
}
