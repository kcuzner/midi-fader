//! Provides the interface to the midi-fader device
//!
//! This abstracts away the OS-layer HID implementation

use std::{fmt, error, io, result, path};
use std::io::{Read, Write};
use std::marker::PhantomData;
use mio;
use tokio;
use tokio::prelude::*;
use tokio::reactor::PollEvented2;

const VID: u16 = 0x16c0;
const PID: u16 = 0x05dc;

#[cfg(target_os="linux")]
mod device_udev;

#[cfg(target_os="linux")]
use self::device_udev as device_impl;

error_chain! {
    foreign_links {
        Io(io::Error);
    }
    links {
        ImplError(device_impl::Error, device_impl::ErrorKind);
    }
}

/// All reports for our devices are 64 bytes long
pub struct Report(pub [u8; 64]);

impl Report {
    pub fn new() -> Self {
        Report([0; 64])
    }
}

pub trait Identified {
    const VID: u16;
    const PID: u16;
    const MANUFACTURER: &'static str;
    const PRODUCT: &'static str;
}

pub struct Device<T: Identified> {
    _0: PhantomData<T>,
    io: tokio::reactor::PollEvented2<device_impl::EventedFile>,
}

impl<T: Identified> Device<T> {
    pub(self) fn new(file: device_impl::EventedFile) -> Self {
        Device { _0: PhantomData, io: PollEvented2::new(file) }
    }

    pub fn poll_read(&mut self, report: &mut Report) -> Result<Async<()>> {
        try_ready!(self.io.poll_read_ready(mio::Ready::readable()));

        match self.io.get_mut().read(&mut report.0) {
            Ok(n) => Ok(().into()),
            Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {
                self.io.clear_read_ready(mio::Ready::readable())?;
                Ok(Async::NotReady)
            },
            Err(e) => Err(e.into()),
        }
    }
}

impl<T: Identified + 'static> Device<T> {
    pub fn enumerate() -> Result<impl Iterator<Item=Result<Self>>> {
        let it = device_impl::DeviceEnumeration::<T>::new()?;
        Ok(it.map(|o| o.open()))
    }
}

pub trait Open<T: Identified> {
    /// Opens the device this represents
    fn open(&self) -> Result<Device<T>>;
}

/// Generates a device path
pub trait DevicePath<T: Identified> {
    /// Reads the system name for the device
    ///
    /// This can be identical to the path. It is used only during error creation
    fn name(&self) -> &path::Path;
    /// Attempts to read the device node path
    fn node(&self) -> Option<&path::Path>;
}

pub struct DeviceIterator<T: Identified> {
    it: device_impl::DeviceEnumeration<T>,
}

/// Midi-Fader device
pub struct MidiFader {
    _0: (),
}

impl Identified for MidiFader {
    const VID: u16 = VID;
    const PID: u16 = PID;
    const MANUFACTURER: &'static str = "kevincuzner.com";
    const PRODUCT: &'static str = "Midi-Fader";
}

/// Midi-Fader bootloader device
pub struct Bootloader {
    _0: (),
}

impl Identified for Bootloader {
    const VID: u16 = VID;
    const PID: u16 = PID;
    const MANUFACTURER: &'static str = "kevincuzner.com";
    const PRODUCT: &'static str = "Midi-Fader Bootloader";
}

