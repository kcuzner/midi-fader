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
use std::{io, ptr, mem};
use alloc::alloc;

pub enum Error {
    #[fail(display = "IO error: {}", _0)]
    Io(io::Error),
}

impl From<io::Error> for Error {
    fn from(e: io::Error) ->Self {
        Error::Io(e)
    }
}

type Result<T> = std::result::Result<T, Error>;

struct DeviceInterfaceDetailData {
    layout: alloc::Layout,
    pointer: *mut u8,
}

impl DeviceInterfaceDetailData {
    fn new()
}

impl Drop for DeviceInterfaceDetailData {
    fn drop(&mut self) {
        unsafe { alloc::dealloc(self.pointer, self.layout) }
    }
}

// Scanning devices is done by class GUID. Supposedly this is the one for HIDs.
const HID_CLASS_GUID: shared::guiddef::GUID = shared::guiddef::GUID {
    Data1: 0x4d1e55b2,
    Data2: 0xf16f,
    Data3: 0x11cf,
    Data4: [0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30],
};

pub(super) struct DeviceEnumeration<T: Identified> {
    _0: PhantomData<T>,
    infoset: setupapi::HDEVINFO,
    device_index: u32,
}

impl<T: Identified> DeviceEnumeration<T> {
    pub fn new() -> Result<Self> {
        let infoset = unsafe { setupapi::SetupDiGetClassDevsA(&HID_CLASS_GUID as *const _, ptr::null(), ptr::null_mut(),
            setupapi::DIGCF_PRESENT | setupapi::DIGCF_DEVICEINTERFACE) };
        Ok(DeviceEnumeration { _0: PhantomData, infoset: infoset, device_index: 0, })
    }
}

impl<T: Identified + 'static> Iterator for DeviceEnumeration<T> {
    type Item = Box<Open<T>>;

    fn next(&mut self) -> Option<Self::Item> {
        let mut did = setupapi::SP_DEVICE_INTERFACE_DATA {
            cbSize: mem::size_of::<setupapi::SP_DEVICE_INTERFACE_DATA>() as u32,
            InterfaceClassGuid: HID_CLASS_GUID,
            Flags: 0,
            Reserved: 0usize,
        };
        if unsafe { setupapi::SetupDiEnumDeviceInterfaces(self.infoset, ptr::null_mut(), &HID_CLASS_GUID as *const _,
            self.device_index, &mut did as *mut _) } == 0 {
            // End of the list
            return None;
        }
        let mut required_size = 0u32;
        if unsafe { setupapi::SetupDiGetDeviceInterfaceDetailA(self.infoset, &mut did as *mut _, ptr::null_mut(), 0,
            &mut required_size as *mut _, ptr::null_mut()) } == 0 {
            return None;
        }
        // Do a very unsafe allocation
        let mut 
        None
    }
}

impl<T: Identified> Drop for DeviceEnumeration<T> {
    fn drop(&mut self) {
        // Clean up our infoset
        if unsafe { setupapi::SetupDiDestroyDeviceInfoList(self.infoset) } == 0 {
            panic!("Error while destroying device infoset");
        }
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
    /// Reads this device
    ///
    /// Note that this does not require exclusive access to the device.
    pub fn read(&self, buf: &mut [u8]) -> Result<usize> {
        unimplemented!();
    }

    /// Writes this device
    ///
    /// The passed buffer must be the exact length of the report which will be sent to the device.
    /// It must be preceded by a single byte which is the report ID or 0x00 if report IDs are not
    /// used.
    ///
    /// Note that this does not require exclusive access to the device.
    ///
    /// TODO: Make the extra report ID an abstraction so I don't have to worry about it explicitly.
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
