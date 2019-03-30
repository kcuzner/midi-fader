//! Device implementation for windows
//!
//! This is heavily based on hidapi by Signal11, but I did not actually include
//! that library since I instead wanted to favor a 100% rust implementation.
//!
//! See https://github.com/signal11/hidapi/blob/master/windows/hid.c

use winapi::shared;
use winapi::um::setupapi;

use device;
use device::{Identified, Device, Open};

use std::marker::PhantomData;
use std::io;

error_chain! {
    foreign_links {
        Io(io::Error);
    }
}

const HID_CLASS_GUID: shared::guiddef::GUID = shared::guiddef::GUID {
    Data1: 0x4d1e55b2,
    Data2: 0xf16f,
    Data3: 0x11cf,
    Data4: [0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30],
};

pub(super) struct DeviceEnumeration<T: Identified> {
    _0: PhantomData<T>,
    infoset: setupapi::HDEVINFO,
}

impl<T: Identified> DeviceEnumeration<T> {
    pub fn new() -> Result<Self> {
        unimplemented!()
    }
}

impl<T: Identified + 'static> Iterator for DeviceEnumeration<T> {
    type Item = Box<Open<T>>;

    fn next(&mut self) -> Option<Self::Item> {
        None
    }
}

/// Human Interface Device abstraction implementation
///
/// The human interface device can be read or written concurrently
#[derive(Debug)]
pub(super) struct HidDevice {
    _0: ()
}

impl HidDevice {
    pub fn read(&self, buf: &mut [u8]) -> Result<usize> {
        unimplemented!();
    }

    pub fn write(&self, buf: &[u8]) -> Result<usize> {
        unimplemented!();
    }
}

impl mio::Evented for HidDevice {
    fn register(&self, poll: &mio::Poll, token: mio::Token, interest: mio::Ready, opts: mio::PollOpt) -> io::Result<()> {
        unimplemented!();
    }
    fn reregister(&self, poll: &mio::Poll, token: mio::Token, interest: mio::Ready, opts: mio::PollOpt) -> io::Result<()> {
        unimplemented!();
    }
    fn deregister(&self, poll: &mio::Poll) -> io::Result<()> {
        unimplemented!();
    }
}
