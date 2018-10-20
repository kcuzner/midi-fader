//! Provides the interface to the midi-fader device
//!
//! This abstracts away the OS-layer HID implementation

use byteorder::{ByteOrder, LittleEndian};
use std::{fmt, io, self};
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
        Utf8(std::str::Utf8Error);
    }
    links {
        ImplError(os::Error, os::ErrorKind);
    }
    errors {
        DeviceError(n: i32) {
            description("Error from the device"),
            display("Error from the device: {}", n),
        }
        ParameterSizeError(s: usize) {
            description("The parameter was of an invalid size"),
            display("The parameter was of an invalid size: {}", s),
        }
        UnexpectedResponseError {
            description("Unexpected device response"),
            display("Unexpected device response"),
        }
    }
}

pub trait Identified {
    const VID: u16;
    const PID: u16;
    const MANUFACTURER: &'static str;
    const PRODUCT: &'static str;
}

/// Human interface device with asynchronous reads
pub trait AsyncHidDevice<T: Identified>: Sized {
    /// Perform an asynchronous read
    fn read<B>(self, report: B) -> ReadReport<T, Self, B>
        where B: AsMut<[u8]>;
    /// Poll for a read
    fn poll_read(&self, report: &mut [u8]) -> Result<Async<usize>>;
    /// Perform a synchronous write
    fn write(&self, report: &[u8]) -> Result<usize>;
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

}

impl<T: Identified> AsyncHidDevice<T> for Device<T> {
    /// Polls the read status of the inner HidDevice
    fn poll_read(&self, report: &mut [u8]) -> Result<Async<usize>> {
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
    fn read<B>(self, report: B) -> ReadReport<T, Self, B>
        where B: AsMut<[u8]>
    {
        ReadReport::new(self, report)
    }

    /// Writes to the device.
    ///
    /// This is not a promise because (at least on Linux) the raw HID device will never return
    /// EAGAIN. It also never seems to signal that it's ready for writing.
    fn write(&self, report: &[u8]) -> Result<usize>
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
struct ReadReportInner<I: Identified + 'static, T: AsyncHidDevice<I>, B: AsMut<[u8]>> {
    _0: PhantomData<I>,
    device: T,
    buf: B,
}

#[derive(Debug)]
pub struct ReadReport<I: Identified + 'static, T: AsyncHidDevice<I>, B: AsMut<[u8]>> {
    /// None means the future was completed
    state: Option<ReadReportInner<I, T, B>>,
}


impl<I: Identified + 'static, T: AsyncHidDevice<I>, B: AsMut<[u8]>> ReadReport<I, T, B> {
    fn new(dev: T, buf: B) -> Self {
        // I'm borrowing this pattern from the UdpSocket example, but I'm not sure I agree with the
        // choice to have ReadReportInner not have a new. But, maybe its just simpler that way.
        let inner = ReadReportInner { _0: PhantomData, device: dev, buf: buf };
        ReadReport { state: Some(inner) }
    }
}

impl<I: Identified + 'static, T: AsyncHidDevice<I>, B: AsMut<[u8]>> Future for ReadReport<I, T, B> {
    type Item = (T, B, usize);
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

    /// Sets a word in these args
    fn set_word(mut self, index: usize, value: u32) -> Result<Self> {
        let index = size_of::<u32>() * index + 1;
        match index {
            i if i < self.data.len()-size_of::<u32>() => {
                LittleEndian::write_u32(&mut self.data[i..i+size_of::<u32>()], value);
            },
            _ => {},
        }
        Ok(self)
    }

    /// Gets the command portion of this command
    pub fn command(&self) -> u32 {
        self.get_word(0).unwrap()
    }

    pub fn set_command(self, command: u32) -> Result<Self> {
        self.set_word(0, command)
    }

    /// Gets a parameter at the passed index for this command
    pub fn parameter(&self, index: usize) -> Option<u32> {
        self.get_word(index+1)
    }

    pub fn set_parameter(self, index: usize, value: u32) -> Result<Self> {
        self.set_word(index+1, value)
    }

    /// Gets the version string located in bytes 8-63 of the report
    fn version(&self) -> Result<&str> {
        std::str::from_utf8(&self.data[9..65]).map_err(|e| e.into())
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
struct MidiFaderCommandInner<T: AsyncHidDevice<MidiFader>> {
    device: T,
    data: MidiFaderCommandArgs,
}

#[derive(Debug)]
enum MidiFaderCommandState<T: AsyncHidDevice<MidiFader>> {
    Command(Option<MidiFaderCommandInner<T>>),
    Status(ReadReport<MidiFader, T, MidiFaderCommandArgs>),
}

/// Command for the midi fader device
///
/// All communication with a MidiFader is done in a command-response format. This future
/// encapsulates that format.
#[derive(Debug)]
pub struct MidiFaderCommand<T: AsyncHidDevice<MidiFader>> {
    state: Option<MidiFaderCommandState<T>>,
}

impl<T: AsyncHidDevice<MidiFader>> MidiFaderCommand<T> {
    /// Creates a new midi fader command
    pub fn new(dev: T, args: MidiFaderCommandArgs) -> Self {
        MidiFaderCommand { state: Some(MidiFaderCommandState::Command(Some(MidiFaderCommandInner { device: dev, data: args }))) }
    }
}

impl<T: AsyncHidDevice<MidiFader>> Future for MidiFaderCommand<T> {
    type Item = (T, MidiFaderCommandArgs, usize);
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
            let mut inner = self.state.as_mut().unwrap();
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

/// Attached midi fader device status
pub struct DeviceStatus {
    channels: u32,
    version: String,
}

impl DeviceStatus {
    fn new(channels: u32, version: &str) -> Self {
        DeviceStatus { channels: channels, version: version.to_owned() }
    }

    pub fn channels(&self) -> u32 {
        self.channels
    }

    pub fn version(&self) -> &str {
        &self.version
    }
}

/// Signed parameter value with a size attached
pub struct ParameterValue {
    value: i32,
    size: usize,
}

impl ParameterValue {
    pub fn new(value: i32, size: usize) -> Self {
        ParameterValue { value: value, size: size }
    }

    pub fn value(&self) -> i32 {
        self.value
    }

    pub fn size(&self) -> usize {
        self.size
    }
}

impl From<ParameterValue> for i32 {
    fn from(val: ParameterValue) -> i32 {
        val.value
    }
}

/// Command for getting the device status from the midi fader device
pub struct Status<T: AsyncHidDevice<MidiFader>> {
    command: Option<MidiFaderCommand<T>>,
}

impl<T: AsyncHidDevice<MidiFader>> Status<T> {
    pub fn new(device: T) -> Self {
        let args = MidiFaderCommandArgs::new().
            set_command(0x00).unwrap();
        Status { command: Some(MidiFaderCommand::new(device, args)) }
    }
}

impl<T: AsyncHidDevice<MidiFader>> Future for Status<T> {
    type Item = (T, DeviceStatus);
    type Error = Error;

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        const MAGIC: u32 = 0xDEADBEEF;

        // Attempt to complete the command
        let result = {
            let mut inner = self.command.as_mut().expect("Command polled after completion");
            try_ready!(inner.poll())
        };

        // The command is completed
        self.command.take().unwrap();
        match result.1.parameter(0).unwrap() as u32 {
            n if n != MAGIC => return Err(ErrorKind::UnexpectedResponseError.into()),
            _ => {}
        }
        let status = DeviceStatus::new(result.1.parameter(1).unwrap(), result.1.version().unwrap());
        Ok(Async::Ready((result.0, status)))
    }
}

/// Command for getting a parameter from the midi fader device
pub struct GetParameter<T: AsyncHidDevice<MidiFader>> {
    command: Option<MidiFaderCommand<T>>,
}

impl<T: AsyncHidDevice<MidiFader>> GetParameter<T> {
    // Build a new get parameter command
    pub fn new(device: T, parameter: u16) -> Self {
        let args = MidiFaderCommandArgs::new().
            set_command(0x40).unwrap().
            set_parameter(0, 0).unwrap().
            set_parameter(1, parameter as u32).unwrap();
        GetParameter { command: Some(MidiFaderCommand::new(device, args)) }
    }
}

impl<T: AsyncHidDevice<MidiFader>> Future for GetParameter<T> {
    type Item = (T, ParameterValue);
    type Error = Error;

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        // Attempt to complete the command
        let result = {
            let mut inner = self.command.as_mut().expect("Command polled after completion");
            try_ready!(inner.poll())
        };

        // The command is completed
        self.command.take().unwrap();
        match result.1.parameter(0).unwrap() as i32 {
            n if n != 0 => return Err(ErrorKind::DeviceError(n).into()),
            _ => {},
        }
        let raw = result.1.parameter(2).unwrap();
        let size = result.1.parameter(3).unwrap() as usize;
        let value = match size {
            1 => ((raw as u8) as i8) as i32,
            2 => {
                let mut buf = [0u8; 2];
                LittleEndian::write_u16(&mut buf[..], raw as u16);
                LittleEndian::read_i16(&buf[..]) as i32
            },
            4 => raw as i32,
            v => return Err(ErrorKind::ParameterSizeError(v).into()),
        };
        Ok(Async::Ready((result.0, ParameterValue::new(value, size))))
    }
}

pub struct SetParameter<T: AsyncHidDevice<MidiFader>> {
    command: Option<MidiFaderCommand<T>>,
}

impl<T: AsyncHidDevice<MidiFader>> SetParameter<T> {
    // Build a new set parameter command
    pub fn new(device: T, parameter: u16, value: ParameterValue) -> Self {
        match value.size() {
            1 | 2 | 4 => {},
            _ => panic!("Invalid parameter value size"),
        }
        let args = MidiFaderCommandArgs::new().
            set_command(0x80).unwrap().
            set_parameter(0, 0).unwrap().
            set_parameter(1, parameter as u32).unwrap().
            set_parameter(2, value.value() as u32).unwrap().
            set_parameter(3, value.size() as u32).unwrap();
        SetParameter { command: Some(MidiFaderCommand::new(device, args)) }
    }
}

impl<T: AsyncHidDevice<MidiFader>> Future for SetParameter<T> {
    type Item = T;
    type Error = Error;

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        // Attempt to complete the command
        let result = {
            let mut inner = self.command.as_mut().expect("Command polled after completion");
            try_ready!(inner.poll())
        };

        // The command is completed
        self.command.take().unwrap();
        match result.1.parameter(0).unwrap() as i32 {
            n if n != 0 => return Err(ErrorKind::DeviceError(n).into()),
            _ => {},
        }
        Ok(Async::Ready(result.0))
    }
}

/// Extensions for talking to the midi fader device
///
/// These are implemented for all types that implement AsyncHidDevice<MidiFader>
pub trait MidiFaderExtensions<T: AsyncHidDevice<MidiFader>> {
    /// Gets the device status
    fn device_status(self) -> Status<T>;
    /// Gets a device parameter
    fn get_parameter(self, parameter: u16) -> GetParameter<T>;
    /// Sets a device parameter
    fn set_parameter(self, parameter: u16, value: ParameterValue) -> SetParameter<T>;
}

impl<T: AsyncHidDevice<MidiFader>> MidiFaderExtensions<T> for T {
    fn device_status(self) -> Status<T> {
        Status::new(self)
    }
    fn get_parameter(self, parameter: u16) -> GetParameter<T> {
        GetParameter::new(self, parameter)
    }
    fn set_parameter(self, parameter: u16, value: ParameterValue) -> SetParameter<T> {
        SetParameter::new(self, parameter, value)
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

