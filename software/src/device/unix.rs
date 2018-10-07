//! Device implementation for linix that uses udev
//!
//! This is heavily based on hidapi by Signal11, but I did not actually include
//! that library since I instead wanted to favor a 100% rust implementation.
//!
//! See https://github.com/signal11/hidapi/blob/master/linux/hid.c

use device;
use device::{Identified, Device, Open};
use errno::{errno};
use libc;
use mio;
use udev;

use std::marker::PhantomData;
use std::{ffi, path, io};
use std::os::unix;



error_chain! {
    foreign_links {
        Udev(::udev::Error);
        Io(io::Error);
    }
    errors {
        NoDeviceNode(syspath: String) {
            description("No device node for device"),
            display("No device node for '{}'", syspath),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
enum BusType {
    Pci = 0x01,
    IsaPnP = 0x02,
    Usb = 0x03,
    Hil = 0x04,
    Bluetooth = 0x05,
    Virtual = 0x06
}

impl BusType {
    fn from_u32(value: u32) -> Option<BusType> {
        match value {
            v if v == BusType::Pci as u32 => Some(BusType::Pci),
            v if v == BusType::IsaPnP as u32 => Some(BusType::IsaPnP),
            v if v == BusType::Usb as u32 => Some(BusType::Usb),
            v if v == BusType::Hil as u32 => Some(BusType::Hil),
            v if v == BusType::Bluetooth as u32 => Some(BusType::Bluetooth),
            v if v == BusType::Virtual as u32 => Some(BusType::Virtual),
            _ => None
        }
    }
}

#[derive(Debug)]
struct UEventInfo {
    bus_type: BusType,
    vendor_id: u16,
    product_id: u16,
    product_name: String,
    serial_number: String,
}

impl UEventInfo {
    /// Parses a uevent string into a UEventInfo struct
    ///
    /// This returns None if any field is not found
    fn new(uevent: &ffi::OsStr) -> Option<Self> {
        let mut bus_type: Option<BusType> = None;
        let mut vendor_id: Option<u16> = None;
        let mut product_id: Option<u16> = None;
        let mut product_name: Option<String> = None;
        let mut serial_number: Option<String> = None;
        for line in uevent.to_str().unwrap().split("\n") {
            let parts: Vec<&str> = line.split('=').collect();
            if parts.len() != 2 {
                continue;
            }
            match parts[0] {
                "HID_ID" => {
                    let numbers: Vec<u32> = parts[1].split(':').map(|x| u32::from_str_radix(x, 16).unwrap()).collect();
                    if numbers.len() != 3 {
                        continue;
                    }
                    bus_type = BusType::from_u32(numbers[0]);
                    vendor_id = Some(numbers[1] as u16);
                    product_id = Some(numbers[2] as u16);
                },
                "HID_NAME" => {
                    product_name = Some(parts[1].to_owned());
                },
                "HID_UNIQ" => {
                    serial_number = Some(parts[1].to_owned());
                }
                _ => continue,
            }
        }
        match (bus_type, vendor_id, product_id, product_name, serial_number) {
            (Some(b), Some(vid), Some(pid), Some(pn), Some(sn)) => Some(UEventInfo {
                bus_type: b,
                vendor_id: vid,
                product_id: pid,
                product_name: pn,
                serial_number: sn,
            }),
            _ => None
        }
    }
}

/// Gets a parent device, discarding any error
fn get_parent_device(dev: &udev::Device, subsystem: &path::Path) -> Option<udev::Device> {
    match dev.parent_with_subsystem(subsystem) {
        Ok(opt) => opt,
        Err(_) => None,
    }
}

/// Gets a parent device, discarding any error
fn get_parent_device_devtype(dev: &udev::Device, subsystem: &path::Path, devtype: &path::Path) -> Option<udev::Device> {
    match dev.parent_with_subsystem_devtype(subsystem, devtype) {
        Ok(opt) => opt,
        Err(_) => None,
    }
}

#[derive(Debug)]
struct DeviceDetails {
    manufacturer: String,
    product: String,
}

impl DeviceDetails {
    fn new(raw_dev: &udev::Device, uevent: &UEventInfo) -> Option<Self> {
        match uevent.bus_type {
            BusType::Usb => {
                let usb_dev = get_parent_device_devtype(raw_dev, &path::Path::new("usb"), &path::Path::new("usb_device"))?;
                let manufacturer = usb_dev.attribute_value("manufacturer")?.to_str()?.to_owned();
                let product = usb_dev.attribute_value("product")?.to_str()?.to_owned();
                Some(DeviceDetails { manufacturer: manufacturer, product: product })
            },
            BusType::Bluetooth => {
                Some(DeviceDetails { manufacturer: String::new(), product: uevent.product_name.clone() })
            }
            _ => None,
        }
    }
}

pub(super) struct DeviceEnumeration<T: Identified> {
    _0: PhantomData<T>,
    iter: udev::Devices,
}

impl<T: Identified> DeviceEnumeration<T> {
    pub fn new() -> Result<Self> {
        let context = udev::Context::new()?;
        let mut enumerator = udev::Enumerator::new(&context)?;
        enumerator.match_is_initialized()?;
        enumerator.match_subsystem("hidraw")?;
        let devs = enumerator.scan_devices()?;
        Ok(DeviceEnumeration { _0: PhantomData, iter: devs })
    }

    fn filter_device(dev: udev::Device) -> Option<udev::Device> {
        // Match the device
        let hid_dev = get_parent_device(&dev, &path::Path::new("hid"))?;
        let uevent = UEventInfo::new(hid_dev.attribute_value("uevent")?)?;
        if !(uevent.bus_type == BusType::Usb || uevent.bus_type == BusType::Bluetooth) || uevent.vendor_id != T::VID || uevent.product_id != T::PID {
            return None;
        }
        let details = DeviceDetails::new(&dev, &uevent)?;
        if details.manufacturer != T::MANUFACTURER || details.product != T::PRODUCT {
            return None;
        }
        Some(dev)
    }
}

impl<T: Identified + 'static> Iterator for DeviceEnumeration<T> {
    type Item = Box<Open<T>>;

    fn next(&mut self) -> Option<Self::Item> {
        // Read the next device in the udev enumeration
        while let Some(dev) = self.iter.next() {
            match DeviceEnumeration::<T>::filter_device(dev) {
                Some(dev) => return Some(Box::new(OpenUdev::<T>::new(dev))),
                None => continue,
            };
        }
        None
    }
}

struct OpenUdev<T: Identified> {
    _0: PhantomData<T>,
    dev: udev::Device,
}

impl<T: Identified> OpenUdev<T> {
    fn new(dev: udev::Device) -> Self {
        OpenUdev { _0: PhantomData, dev: dev }
    }
}

impl<T: Identified> Open<T> for OpenUdev<T> {
    fn open(&self) -> device::Result<Device<T>> {
        let node = self.dev.devnode()
            .map_or_else(|| Err(Error::from_kind(ErrorKind::NoDeviceNode(self.dev.syspath().to_str().unwrap().to_owned()))), |n| Ok(n))?;
        let hid_device = HidDevice::new(node)?;
        Ok(Device::new(hid_device))
    }
}

/// Human Interface Device abstraction implementation
///
/// The human interface device can be read and written concurrently.
pub(super) struct HidDevice {
    fd: unix::io::RawFd,
}

impl HidDevice {
    /// Creates a new HidDevice from a device node path
    fn new(node: &path::Path) -> Result<Self> {
        let raw_path = node.to_str().unwrap();
        match unsafe { libc::open(raw_path.as_ptr() as *const i8, libc::O_RDWR | libc::O_NONBLOCK) } {
            -1 => Err(io::Error::from(errno()).into()),
            fd => Ok(HidDevice { fd: fd }),
        }
    }

    /// Reads this device
    ///
    /// Note that this does not require exclusive access to the device.
    pub fn read(&self, buf: &mut [u8]) -> Result<usize> {
        match unsafe { libc::read(self.fd, buf as *mut _ as *mut libc::c_void, buf.len()) } {
            -1 => Err(io::Error::from(errno()).into()),
            size => Ok(size as usize),
        }
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
        match unsafe { libc::write(self.fd, buf as *const _ as *const libc::c_void, buf.len()) } {
            -1 => Err(io::Error::from(errno()).into()),
            size => Ok(size as usize),
        }
    }
}

impl Drop for HidDevice {
    /// Closes our underlying file descriptor
    fn drop(&mut self) {
        if unsafe { libc::close(self.fd) } != 0 {
            // I'm not sure this is the right thing to do, but a close failing is pretty
            // catastrophic
            let err = errno();
            panic!("Error while closing file descriptor {}", err);
        }
    }
}

impl mio::Evented for HidDevice {
    fn register(&self, poll: &mio::Poll, token: mio::Token, interest: mio::Ready, opts: mio::PollOpt) -> io::Result<()> {
        mio::unix::EventedFd(&self.fd).register(poll, token, interest, opts)
    }
    fn reregister(&self, poll: &mio::Poll, token: mio::Token, interest: mio::Ready, opts: mio::PollOpt) -> io::Result<()> {
        mio::unix::EventedFd(&self.fd).reregister(poll, token, interest, opts)
    }
    fn deregister(&self, poll: &mio::Poll) -> io::Result<()> {
        mio::unix::EventedFd(&self.fd).deregister(poll)
    }
}

