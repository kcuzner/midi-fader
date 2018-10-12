//! Provides the interface to the midi-fader device
//!
//! This abstracts away the OS-layer HID implementation

use byteorder::{ByteOrder, LittleEndian};
use std::{fmt, io};
use std::marker::PhantomData;
use std::mem::size_of;
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
#[derive(Debug)]
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
    pub fn poll_read(&self, report: &mut [u8]) -> Result<Async<usize>> {
        try_ready!(self.io.poll_read_ready(mio::Ready::readable()));

        match self.io.get_ref().read(report) {
            Ok(n) => Ok(n.into()),
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
    /// TODO: Make it possible to do concurrent reads and writes.
    /// The issue right now is the lifetimes make things freak out. I also think I'll run into
    /// issues with borrow-across-yield.
    pub fn read<B>(self, report: B) -> ReadReport<T, B>
        where B: AsMut<[u8]>
    {
        ReadReport::new(self, report)
    }

    /// Writes to the device.
    ///
    /// This is not a promise because (at least on Linux) the raw HID device will never return
    /// EAGAIN. It also never seems to signal that it's ready for writing.
    pub fn write(&self, report: &[u8]) -> Result<usize>
    {
        self.io.get_ref().write(report).map_err(|e| e.into())
    }
}

impl<T: Identified + 'static> Device<T> {
    pub fn enumerate() -> Result<impl Iterator<Item=Result<Self>>> {
        let it = os::DeviceEnumeration::<T>::new()?;
        Ok(it.map(|o| o.open()))
    }
}

#[derive(Debug)]
struct ReadReportInner<T: Identified + 'static, B: AsMut<[u8]>> {
    device: Device<T>,
    buf: B,
}

#[derive(Debug)]
pub struct ReadReport<T: Identified + 'static, B: AsMut<[u8]>> {
    /// None means the future was completed
    state: Option<ReadReportInner<T, B>>,
}


impl<T: Identified + 'static, B: AsMut<[u8]>> ReadReport<T, B> {
    fn new(dev: Device<T>, buf: B) -> Self {
        // I'm borrowing this pattern from the UdpSocket example, but I'm not sure I agree with the
        // choice to have ReadReportInner not have a new. But, maybe its just simpler that way.
        let inner = ReadReportInner { device: dev, buf: buf };
        ReadReport { state: Some(inner) }
    }
}

impl<T: Identified + 'static, B: AsMut<[u8]>> Future for ReadReport<T, B> {
    type Item = (Device<T>, B, usize);
    type Error = Error;

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        let n = {
            let ref mut inner = self.state.as_mut().expect("ReadReport polled after completion");

            try_ready!(inner.device.poll_read(inner.buf.as_mut()))
        };

        let inner = self.state.take().unwrap();
        Ok(Async::Ready((inner.device, inner.buf, n)))
    }
}

pub trait Open<T: Identified> {
    /// Opens the device this represents
    fn open(&self) -> Result<Device<T>>;
}

/// Midi-Fader device
#[derive(Debug)]
pub struct MidiFader {
    _0: (),
}

impl Identified for MidiFader {
    const VID: u16 = VID;
    const PID: u16 = PID;
    const MANUFACTURER: &'static str = "kevincuzner.com";
    const PRODUCT: &'static str = "Midi-Fader";
}

/// Generic command report for the MidiFader device (not the bootloader)
///
/// This implements the AsRef and AsMut traits in such a way as to make this suitable for use as
/// both a read and write report.
pub struct MidiFaderCommandArgs {
    data: [u8; 65],
}

impl MidiFaderCommandArgs {
    /// Creates new empty command arguments
    pub fn new() -> Self {
        MidiFaderCommandArgs { data: [0u8; 65] }
    }

    /// Gets a word from our buffer
    fn get_word(&self, index: usize) -> Option<u32> {
        let index = size_of::<u32>() * index + 1;
        match index {
            i if i < self.data.len()-size_of::<u32>() => Some(LittleEndian::read_u32(&self.data[i..i+size_of::<u32>()])),
            _ => None,
        }
    }

    /// Gets the command portion of this command
    pub fn command(&self) -> u32 {
        self.get_word(0).unwrap()
    }

    /// Gets a parameter at the passed index for this command
    pub fn parameter(&self, index: usize) -> Option<u32> {
        self.get_word(index+1)
    }
}

impl AsRef<[u8]> for MidiFaderCommandArgs {
    /// Exposes this report as a buffer for writing
    fn as_ref(&self) -> &[u8] {
        // Include the report number when writing
        &self.data
    }
}

impl AsMut<[u8]> for MidiFaderCommandArgs {
    /// Exposes this report as a buffer for reading
    fn as_mut(&mut self) -> &mut [u8] {
        // We don't have a report number, so we only get 64 bytes when a report is read
        &mut self.data[1..]
    }
}

impl fmt::Debug for MidiFaderCommandArgs {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let params: Vec<u32> = { 0..14usize }
            .map(|i| self.parameter(i).unwrap()).collect();
        let command = self.command();
        write!(f, "MidiFaderCommandArgs {{ cmd: {:?}, params: {:?} }}", command, params)
    }
}

#[derive(Debug)]
struct MidiFaderCommandInner {
    device: Device<MidiFader>,
    data: MidiFaderCommandArgs,
}

#[derive(Debug)]
enum MidiFaderCommandState {
    Command(Option<MidiFaderCommandInner>),
    Status(ReadReport<MidiFader, MidiFaderCommandArgs>),
}

#[derive(Debug)]
pub struct MidiFaderCommand {
    state: Option<MidiFaderCommandState>,
}

impl MidiFaderCommand {
    pub fn new(dev: Device<MidiFader>, args: MidiFaderCommandArgs) -> Self {
        MidiFaderCommand { state: Some(MidiFaderCommandState::Command(Some(MidiFaderCommandInner { device: dev, data: args }))) }
    }
}

impl Future for MidiFaderCommand {
    type Item = (Device<MidiFader>, MidiFaderCommandArgs, usize);
    type Error = Error;

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        let mut state = self.state.take().expect("MidiFaderCommand polled after completion");
        // Run the initial write if needed
        state = if let MidiFaderCommandState::Command(ref mut inner) = state {
            let inner = inner.take().unwrap();
            inner.device.write(inner.data.as_ref())?;
            MidiFaderCommandState::Status(inner.device.read(inner.data))
        } else { state };
        self.state = Some(state);
        // Poll the underlying read
        let item = {
            let mut inner = self.state.as_mut().expect("MidiFaderCommand polled after completion");
            match inner {
                MidiFaderCommandState::Status(ref mut read) => {
                    try_ready!(read.poll())
                },
                _ => panic!("MidiFaderCommand in unexpected state"),
            }
        };

        // The command is now finished
        self.state.take();
        Ok(Async::Ready(item))
    }
}

/// Midi-Fader bootloader device
#[derive(Debug)]
pub struct Bootloader {
    _0: (),
}

impl Identified for Bootloader {
    const VID: u16 = VID;
    const PID: u16 = PID;
    const MANUFACTURER: &'static str = "kevincuzner.com";
    const PRODUCT: &'static str = "Midi-Fader Bootloader";
}

