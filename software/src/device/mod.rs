//! Provides the interface to the midi-fader device
//!
//! This abstracts away the OS-layer HID implementation

use std::{fmt, error, io, result, path};
use std::io::{Read, Write};
use std::fs::File;

const VID: u16 = 0x16c0;
const PID: u16 = 0x05dc;

#[cfg(target_os="linux")]
mod device_udev;

#[cfg(target_os="linux")]
use self::device_udev as device_impl;

error_chain! {
    links {
        ImplError(device_impl::Error, device_impl::ErrorKind);
    }
}

pub trait Device {
    const VID: u16;
    const PID: u16;
    const MANUFACTURER: &'static str;
    const PRODUCT: &'static str;
}

pub trait DeviceInstance<T: Device>: Read + Write {
}

/// Midi-Fader device
pub struct MidiFaderDevice {
    underlying: File,
}

impl MidiFaderDevice {
    /// Enumerates all the midi fader devices in the system
    pub fn enumerate() ->Result<impl Iterator<Item=device_impl::DeviceResult<MidiFaderDevice>>> {
        device_impl::DeviceEnumeration::<MidiFaderDevice>::new().map_err(|e| e.into())
    }
}

impl Device for MidiFaderDevice {
    const VID: u16 = VID;
    const PID: u16 = PID;
    const MANUFACTURER: &'static str = "kevincuzner.com";
    const PRODUCT: &'static str = "Midi-Fader";
}

/// Midi-Fader bootloader device
pub struct BootloaderDevice {
    underlying: File,
}

impl BootloaderDevice {
    /// Enumerates all the bootloader devices in the system
    pub fn enumerate() -> Result<impl Iterator<Item=device_impl::DeviceResult<BootloaderDevice>>> {
        device_impl::DeviceEnumeration::<BootloaderDevice>::new().map_err(|e| e.into())
    }
}

impl Device for BootloaderDevice {
    const VID: u16 = VID;
    const PID: u16 = PID;
    const MANUFACTURER: &'static str = "kevincuzner.com";
    const PRODUCT: &'static str = "Midi-Fader Bootloader";
}

