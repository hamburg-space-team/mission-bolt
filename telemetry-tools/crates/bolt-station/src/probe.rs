//! Debugger-side answers: is an ST-Link plugged in, which board is it on
//! (STM32 unique-ID -> board map), and reads through OpenOCD's TCL port.

use std::io::{Read, Write};
use std::net::TcpStream;
use std::time::Duration;

use anyhow::{bail, Context, Result};

/// STM32L4 96-bit unique device ID base address (RM0351).
pub const UID_ADDR: u32 = 0x1FFF_7590;

/// ST-Link present on USB? (vendor 0483 anywhere on the bus)
pub fn stlink_on_usb() -> bool {
    let Ok(entries) = std::fs::read_dir("/sys/bus/usb/devices") else {
        return false;
    };
    for e in entries.flatten() {
        if let Ok(v) = std::fs::read_to_string(e.path().join("idVendor")) {
            if v.trim() == "0483" {
                return true;
            }
        }
    }
    false
}

/// One command over OpenOCD's TCL RPC (0x1a-terminated exchanges).
pub fn openocd_tcl(addr: &str, cmd: &str) -> Result<String> {
    let mut s = TcpStream::connect(addr).with_context(|| format!("openocd tcl {addr}"))?;
    s.set_read_timeout(Some(Duration::from_secs(2)))?;
    s.set_write_timeout(Some(Duration::from_secs(2)))?;
    s.write_all(cmd.as_bytes())?;
    s.write_all(&[0x1a])?;
    let mut out = Vec::new();
    let mut byte = [0u8; 1];
    loop {
        let n = s.read(&mut byte)?;
        if n == 0 {
            bail!("openocd closed the tcl connection");
        }
        if byte[0] == 0x1a {
            break;
        }
        out.push(byte[0]);
        if out.len() > 65536 {
            bail!("openocd tcl reply unreasonably large");
        }
    }
    Ok(String::from_utf8_lossy(&out).into_owned())
}

/// `mdw` a range and parse the words. Works while the target runs - the DAP
/// reads memory without halting the core.
pub fn read_words(tcl_addr: &str, addr: u32, count: usize) -> Result<Vec<u32>> {
    let reply = openocd_tcl(tcl_addr, &format!("mdw 0x{addr:08x} {count}"))?;
    let words = parse_mdw(&reply);
    if words.len() < count {
        bail!("mdw returned {} of {count} words: {reply}", words.len());
    }
    Ok(words)
}

/// Parse OpenOCD `mdw` output: lines of "0xADDR: W0 W1 W2 W3".
pub fn parse_mdw(reply: &str) -> Vec<u32> {
    let mut words = Vec::new();
    for line in reply.lines() {
        let Some((_, rest)) = line.split_once(':') else {
            continue;
        };
        for tok in rest.split_whitespace() {
            if let Ok(w) = u32::from_str_radix(tok.trim_start_matches("0x"), 16) {
                words.push(w);
            }
        }
    }
    words
}

/// Chip UID as a stable lowercase hex string.
pub fn uid_string(words: &[u32]) -> String {
    words
        .iter()
        .map(|w| format!("{w:08x}"))
        .collect::<Vec<_>>()
        .join("")
}

/// One line of uids.conf.
#[derive(Debug, Clone)]
pub struct BoardEntry {
    pub uid: String,
    pub board: String,
    pub noinit: Option<u32>,
}

/// uids.conf: `<uid-hex> <board> [<noinit-addr>]` per line, '#' comments.
/// The bench's knowledge of which chip sits on which board.
pub fn load_uid_map(path: &str) -> Vec<BoardEntry> {
    let Ok(text) = std::fs::read_to_string(path) else {
        return Vec::new();
    };
    text.lines()
        .filter_map(|l| {
            let l = l.split('#').next().unwrap_or("").trim();
            let mut f = l.split_whitespace();
            let uid = f.next()?.to_lowercase();
            let board = f.next()?.to_lowercase();
            let noinit = f
                .next()
                .and_then(|a| u32::from_str_radix(a.trim_start_matches("0x"), 16).ok());
            Some(BoardEntry { uid, board, noinit })
        })
        .collect()
}

pub fn entry_for_uid<'m>(map: &'m [BoardEntry], uid: &str) -> Option<&'m BoardEntry> {
    map.iter().find(|e| e.uid == uid)
}

/// BootState::MAGIC_WORD - the marker flash.sh zeroes to force a cold boot.
pub const MAGIC_WORD: u32 = 0xB017_FEED;
/// PersistentState is 16 bytes.
pub const NOINIT_WORDS: usize = 4;
/// STM32L476 SRAM1. .noinit lands just past .bss, so 32 KB is plenty.
pub const SRAM_BASE: u32 = 0x2000_0000;
pub const SCAN_WORDS: usize = 8 * 1024;

/// Find .noinit by its magic word. The address moves whenever .bss
/// changes size, so a configured one would go stale unnoticed.
pub fn find_noinit(tcl_addr: &str) -> Option<u32> {
    const CHUNK: usize = 2048; // keeps each tcl reply well inside its cap
    let mut done = 0usize;
    while done < SCAN_WORDS {
        let n = CHUNK.min(SCAN_WORDS - done);
        let base = SRAM_BASE + (done as u32) * 4;
        let words = read_words(tcl_addr, base, n).ok()?;
        if let Some(i) = words.iter().position(|w| *w == MAGIC_WORD) {
            return Some(base + (i as u32) * 4);
        }
        done += n;
    }
    None
}

/// Decode the .noinit region, mirroring `PersistentState` in
/// shared/core/boot_state.cpp by offset.
///
/// Without the magic there is no state, only whatever RAM held: say so
/// rather than decode noise into plausible-looking fields.
pub fn decode_noinit(addr: u32, words: &[u32]) -> serde_json::Value {
    let raw: Vec<String> = words.iter().map(|w| format!("0x{w:08x}")).collect();
    let magic = words.first().copied().unwrap_or(0);
    if magic != MAGIC_WORD || words.len() < NOINIT_WORDS {
        return serde_json::json!({
            "address": format!("0x{addr:08x}"),
            "magic": format!("0x{magic:08x}"),
            "valid": false,
            "note": "no BootState magic here - wrong address, or the board cold-booted",
            "raw_words": raw,
        });
    }
    let bytes: Vec<u8> = words.iter().flat_map(|w| w.to_le_bytes()).collect();
    let u16at = |i: usize| u16::from_le_bytes([bytes[i], bytes[i + 1]]);
    let u32at = |i: usize| u32::from_le_bytes([bytes[i], bytes[i + 1], bytes[i + 2], bytes[i + 3]]);
    serde_json::json!({
        "address": format!("0x{addr:08x}"),
        "magic": format!("0x{magic:08x}"),
        "valid": true,
        "raw_words": raw,
        "decoded": {
            "tick": u16at(4),
            "reboot_count": u16at(6),
            "mode": if bytes[8] == 1 { "flight" } else { "test" },
            "lo_latched": bytes[9] != 0,
            // older images left this byte at 0
            "reason": bytes[10],
            "reason_name": if bytes[10] == 0 { "unknown" } else { bolt_codec::model::boot_reason(bytes[10]) },
            "lo_rtc_s": u32at(12),
        },
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn mdw_output_parses() {
        let reply = "0x1fff7590: 0039001d 30365114 30343931 \n";
        let w = parse_mdw(reply);
        assert_eq!(w, vec![0x0039_001d, 0x3036_5114, 0x3034_3931]);
        assert_eq!(uid_string(&w), "0039001d3036511430343931");
    }

    #[test]
    fn uid_map_parses_board_and_optional_address() {
        let f = std::env::temp_dir().join("bolt-uids-test.conf");
        std::fs::write(&f, "# comment\naabb btc 0x200031b0\nccdd exp2\n\n").unwrap();
        let map = load_uid_map(f.to_str().unwrap());
        assert_eq!(map.len(), 2);
        assert_eq!(entry_for_uid(&map, "aabb").unwrap().board, "btc");
        assert_eq!(
            entry_for_uid(&map, "aabb").unwrap().noinit,
            Some(0x2000_31B0)
        );
        // the address is optional - a board can be mapped without one
        assert_eq!(entry_for_uid(&map, "ccdd").unwrap().noinit, None);
        assert!(entry_for_uid(&map, "eeff").is_none());
        std::fs::remove_file(&f).ok();
    }

    #[test]
    fn noinit_decodes_persistent_state() {
        // magic | tick=7, count=5 | mode=1, lo=1, reason=2 (watchdog) | rtc=42
        let words = [MAGIC_WORD, 0x0005_0007, 0x0002_0101, 42];
        let v = decode_noinit(0x2000_31B0, &words);
        assert_eq!(v["valid"], true);
        assert_eq!(v["decoded"]["tick"], 7);
        assert_eq!(v["decoded"]["reboot_count"], 5);
        assert_eq!(v["decoded"]["mode"], "flight");
        assert_eq!(v["decoded"]["lo_latched"], true);
        assert_eq!(v["decoded"]["reason_name"], "watchdog");
        assert_eq!(v["decoded"]["lo_rtc_s"], 42);
    }

    #[test]
    fn an_image_without_the_reason_byte_says_unknown() {
        // pre-change firmware left offset 10 as padding
        let words = [MAGIC_WORD, 0x0005_0007, 0x0000_0100, 0];
        let v = decode_noinit(0x2000_31B0, &words);
        assert_eq!(v["decoded"]["reason_name"], "unknown");
    }

    #[test]
    fn a_live_btc_capture_decodes_to_its_uptime() {
        // read off the bench BTC at 0x200031b0 while it had ~1806 s uptime
        let words = [0xB017_FEED, 0x0000_B50E, 0x3A3A_0000, 0x0000_0000];
        let v = decode_noinit(0x2000_31B0, &words);
        assert_eq!(v["decoded"]["tick"], 46350); // 46350 / 25 Hz = 1854 s
        assert_eq!(v["decoded"]["reboot_count"], 0);
        assert_eq!(v["decoded"]["mode"], "test");
        assert_eq!(v["decoded"]["lo_latched"], false);
    }

    #[test]
    fn a_wrong_address_is_reported_not_decoded() {
        // the EXP1 offset read on a BTC board: no magic, so no fake fields
        let words = [0xDF00_0000, 0x5600_0005, 0x0117_B0EE, 0x080C_4E24];
        let v = decode_noinit(0x2000_1F88, &words);
        assert_eq!(v["valid"], false);
        assert!(v["decoded"].is_null());
    }
}
