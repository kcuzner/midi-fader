//! Provides the interface to the midi-fader device
//!
//! This abstracts away the OS-layer HID implementation

use std::{io, path};
use std::marker::PhantomData;
use mio;
use tokio;
use tokio::prelude::*;
use tokio::reactor::PollEvented2;

const VID: u16 = 0x16c0;
const PID: u16 = 0x05dc;

#[cfg(target_os="linux")]
mod unix;

#[cfg(target_os="linux")]
use self::unix as os;

error_chain! {
    foreign_links {
        Io(io::Error);
    }
    links {
        ImplError(os::Error, os::ErrorKind);
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

/// The Human Interface Device
///
/// This implements an asynchronous model for reading and writing the human interface device.
///
/// TODO: Fix multiple-read-write
/// The fix would be to allow separation of the device into two parts, one for reading and one for
/// writing. Then the read/write functions can consume their individual part. However, both parts
/// need to share the same PollEvented. I'm not sure how to get that working.
pub struct Device<T: Identified> {
    _0: PhantomData<T>,
    io: tokio::reactor::PollEvented2<os::HidDevice>,
}

impl<T: Identified> Device<T> {
    /// Creates a new device around the passed HidDevice
    pub(self) fn new(file: os::HidDevice) -> Self {
        Device { _0: PhantomData, io: PollEvented2::new(file) }
    }

    /// Polls the read status of the inner HidDevice
    pub fn poll_read(&self, report: &mut Report) -> Result<Async<()>> {
        try_ready!(self.io.poll_read_ready(mio::Ready::readable()));

        match self.io.get_ref().read(&mut report.0) {
            Ok(_) => Ok(().into()),
            Err(os::Error(os::ErrorKind::Io(ref e), _)) if e.kind() == io::ErrorKind::WouldBlock => {
                self.io.clear_read_ready(mio::Ready::readable())?;
                Ok(Async::NotReady)
            },
            Err(e) => Err(e.into()),
        }
    }

    /// Creates a future for a read from this device
    ///
    /// Note that this does not obtain an exclusive lock on the Device, so it can be called
    /// concurrently with write.
    ///
    /// TODO: Do we really want to allow multiple outstanding reads on a device?
    /// TODO: Use AsMut
    pub fn read<'a>(&'a self, report: &'a mut Report) -> ReadReport<'a, T>
    {
        ReadReport::new(&self, report)
    }

    /// Polls the write status of the inner HidDevice
    pub fn poll_write(&self, report: &Report) -> Result<Async<()>> {
        try_ready!(self.io.poll_write_ready());

        match self.io.get_ref().write(&report.0) {
            Ok(_) => Ok(().into()),
            Err(os::Error(os::ErrorKind::Io(ref e), _)) if e.kind() == io::ErrorKind::WouldBlock => {
                self.io.clear_write_ready()?;
                Ok(Async::NotReady)
            },
            Err(e) => Err(e.into()),
        }
    }

    /// Creates a future for a write to this device
    ///
    /// Note that this does not obtain an exclusive lock on the Device, so it can be called
    /// concurrently with read.
    ///
    /// TODO: Do we really want to allow multiple outstanding writes on a device?
    pub fn write<'a>(&'a self, report: &'a Report) -> WriteReport<'a, T> {
        unimplemented!()
    }
}

impl<T: Identified + 'static> Device<T> {
    pub fn enumerate() -> Result<impl Iterator<Item=Result<Self>>> {
        let it = os::DeviceEnumeration::<T>::new()?;
        Ok(it.map(|o| o.open()))
    }
}

struct ReadReportInner<'a, T: Identified + 'static> {
    device: &'a Device<T>,
    buf: &'a mut Report,
}

pub struct ReadReport<'a, T: Identified + 'static> {
    /// None means the future was completed
    inner: Option<ReadReportInner<'a, T>>,
}


impl<'a, T: Identified + 'static> ReadReport<'a, T> {
    fn new(dev: &'a Device<T>, buf: &'a mut Report) -> Self {
        // I'm borrowing this pattern from the UdpSocket example, but I'm not sure I agree with the
        // choice to have ReadReportInner not have a new. But, maybe its just simpler that way.
        let inner = ReadReportInner { device: dev, buf: buf };
        ReadReport{ inner: Some(inner) }
    }
}

struct WriteReportInner<'a, T: Identified + 'static> {
    device: &'a Device<T>,
    buf: &'a Report,
}

pub struct WriteReport<'a, T: Identified + 'static> {
    inner: Option<WriteReportInner<'a, T>>,
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
    it: os::DeviceEnumeration<T>,
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

