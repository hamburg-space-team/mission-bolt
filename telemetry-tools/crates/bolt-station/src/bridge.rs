//! The ST-Link V3's bridge GPIOs, used to drive the REXUS LO line on the
//! bench while the real harness is not connected.
//!
//! The bridge is its own USB interface (EP 0x06 out / 0x86 in), separate
//! from the debug interface OpenOCD claims, so both stay open at the same
//! time. Wire format per ST's bridge API: a 16-byte command out, an answer
//! back whose first byte is 0x80 when the ST-Link accepted it.

use std::sync::Mutex;
use std::time::Duration;

use anyhow::{bail, Context, Result};
use rusb::{DeviceHandle, GlobalContext};

const ST_VENDOR: u16 = 0x0483;
const EP_OUT: u8 = 0x06;
const EP_IN: u8 = 0x86;

const BRIDGE_CMD: u8 = 0xFC;
const INIT_GPIO: u8 = 0x60;
const SET_RESET_GPIO: u8 = 0x61;
const READ_GPIO: u8 = 0x62;
const BRIDGE_OK: u8 = 0x80;

const CDB_LEN: usize = 16;
const IO_TIMEOUT: Duration = Duration::from_millis(1000);

/// Pin config byte: bits 1-0 mode, 3-2 speed, 5-4 pull, 6 output type.
/// Output, low speed, no pull, push-pull - a static level needs no more,
/// and push-pull is what lets us drive the line high at all.
const CONF_OUTPUT: u8 = 0x01;

/// Highest GPIO the bridge connector carries.
pub const MAX_GPIO: u8 = 3;

pub struct Bridge {
    handle: DeviceHandle<GlobalContext>,
    iface: u8,
    /// pins already switched to output, so INIT is sent once
    armed: u8,
}

/// The interface number carrying the bridge endpoints, if the device has one.
fn bridge_interface(dev: &rusb::Device<GlobalContext>) -> Option<u8> {
    let cfg = dev.active_config_descriptor().ok()?;
    for iface in cfg.interfaces() {
        for desc in iface.descriptors() {
            if desc.endpoint_descriptors().any(|e| e.address() == EP_OUT) {
                return Some(iface.number());
            }
        }
    }
    None
}

impl Bridge {
    /// First ST-Link on the bus that offers a bridge interface.
    pub fn open() -> Result<Self> {
        for dev in rusb::devices().context("enumerate usb")?.iter() {
            let Ok(desc) = dev.device_descriptor() else {
                continue;
            };
            if desc.vendor_id() != ST_VENDOR {
                continue;
            }
            let Some(iface) = bridge_interface(&dev) else {
                continue; // older ST-Link, or firmware without the bridge
            };
            let handle = dev
                .open()
                .context("open the ST-Link (needs root or a udev rule granting the USB device)")?;
            handle
                .claim_interface(iface)
                .with_context(|| format!("claim bridge interface {iface}"))?;
            return Ok(Self {
                handle,
                iface,
                armed: 0,
            });
        }
        bail!("no ST-Link with a bridge interface on USB")
    }

    fn cmd(&self, cdb: &[u8], answer_len: usize) -> Result<Vec<u8>> {
        let mut out = [0u8; CDB_LEN];
        out[..cdb.len()].copy_from_slice(cdb);
        let n = self.handle.write_bulk(EP_OUT, &out, IO_TIMEOUT)?;
        if n != CDB_LEN {
            bail!("short bridge command write ({n}/{CDB_LEN} bytes)");
        }
        let mut buf = vec![0u8; answer_len];
        let n = self.handle.read_bulk(EP_IN, &mut buf, IO_TIMEOUT)?;
        if n != answer_len {
            bail!("short bridge answer ({n}/{answer_len} bytes)");
        }
        if buf[0] != BRIDGE_OK {
            bail!(
                "ST-Link rejected the bridge command (status 0x{:02x})",
                buf[0]
            );
        }
        Ok(buf)
    }

    /// Switch the masked pins to push-pull outputs. Sent once per pin -
    /// re-initialising would glitch a line already being driven.
    pub fn arm(&mut self, mask: u8) -> Result<()> {
        if self.armed & mask == mask {
            return Ok(());
        }
        let want = self.armed | mask;
        // config is per pin, all four bytes always travel
        self.cmd(
            &[
                BRIDGE_CMD,
                INIT_GPIO,
                want,
                CONF_OUTPUT,
                CONF_OUTPUT,
                CONF_OUTPUT,
                CONF_OUTPUT,
            ],
            2,
        )?;
        self.armed = want;
        Ok(())
    }

    /// Drive the masked pins; a bit set in `levels` is high.
    pub fn write(&mut self, mask: u8, levels: u8) -> Result<()> {
        self.arm(mask)?;
        let answer = self.cmd(&[BRIDGE_CMD, SET_RESET_GPIO, mask, levels & mask], 8)?;
        if answer[2] & mask != 0 {
            bail!(
                "ST-Link could not drive gpio mask 0x{:02x}",
                answer[2] & mask
            );
        }
        Ok(())
    }

    /// Read the masked pins back; bit set means high. Deliberately does not
    /// arm: asking what the line does must never start driving it.
    pub fn read(&self, mask: u8) -> Result<u8> {
        let answer = self.cmd(&[BRIDGE_CMD, READ_GPIO, mask], 8)?;
        if answer[2] & mask != 0 {
            bail!(
                "ST-Link could not read gpio mask 0x{:02x}",
                answer[2] & mask
            );
        }
        Ok(answer[3] & mask)
    }
}

impl Drop for Bridge {
    fn drop(&mut self) {
        // released, never closed: BRIDGE_CLOSE would float the pins and
        // drop LO under a board that is mid-flight
        let _ = self.handle.release_interface(self.iface);
    }
}

/// One bridge pin held at a level across probe replugs.
///
/// An unconfigured GPIO is not neutral: the isolator drives the target side
/// low, and low is what the BTC reads as LO asserted, so every cold start
/// looks like a liftoff. Whoever owns the line therefore has to keep
/// driving it, not merely set it once.
pub struct LoLine {
    slot: Mutex<Option<Bridge>>,
    /// level to (re)apply on a fresh handle; None = never drive unasked
    desired: Mutex<Option<bool>>,
    gpio: u8,
}

impl LoLine {
    /// `idle_high` parks the pin at that level from startup on; None leaves
    /// the line alone until someone asks for it.
    pub fn new(gpio: u8, idle_high: Option<bool>) -> Self {
        Self {
            slot: Mutex::new(None),
            desired: Mutex::new(idle_high),
            gpio,
        }
    }

    pub fn gpio(&self) -> u8 {
        self.gpio
    }

    fn mask(&self) -> u8 {
        1u8 << self.gpio
    }

    /// Run `f` against an open bridge, reopening a handle a replug killed and
    /// re-applying the desired level whenever the handle is new.
    fn with<T>(&self, f: impl FnOnce(&mut Bridge, u8) -> Result<T>) -> Result<T> {
        let mask = self.mask();
        let mut slot = self.slot.lock().unwrap();
        let fresh = slot.is_none();
        if fresh {
            *slot = Some(Bridge::open()?);
        }
        let br = slot.as_mut().expect("just opened");
        let out = (|| {
            if fresh {
                if let Some(high) = *self.desired.lock().unwrap() {
                    br.write(mask, if high { mask } else { 0 })?;
                }
            }
            f(br, mask)
        })();
        if out.is_err() {
            *slot = None; // stale handle: the next call opens a fresh one
        }
        out
    }

    /// Drive the pin and remember the level, so a replug restores it.
    pub fn set(&self, high: bool) -> Result<bool> {
        *self.desired.lock().unwrap() = Some(high);
        self.with(|br, mask| {
            br.write(mask, if high { mask } else { 0 })?;
            Ok(br.read(mask)? & mask != 0)
        })
    }

    /// Report the level without arming: asking must not start driving.
    pub fn get(&self) -> Result<bool> {
        self.with(|br, mask| Ok(br.read(mask)? & mask != 0))
    }

    /// Keeper tick: notice a replug and put the level back. Does nothing
    /// until someone (or --lo-idle-release) has said what the level should be.
    pub fn hold(&self) -> Result<()> {
        if self.desired.lock().unwrap().is_none() {
            return Ok(());
        }
        self.with(|br, mask| br.read(mask).map(|_| ()))
    }
}
