//! Host software GUI for the midi-fader

#[macro_use]
extern crate error_chain;
extern crate errno;
#[macro_use]
extern crate futures;
extern crate libc;
extern crate mio;
extern crate tokio;

#[cfg(target_os="linux")]
extern crate udev;

mod device;

use tokio::prelude::*;
use device::{Device, Identified, Report};

struct HidCommandInner<T: Identified> {
    device: Device<T>,
    buf: Report,
}

struct HidCommand<T: Identified> {
    inner: Option<HidCommandInner<T>>,
}

impl<T: Identified> HidCommand<T> {
    fn new(dev: Device<T>, buf: Report) -> Self {
        let inner = HidCommandInner { device: dev, buf: buf };
        HidCommand { inner: Some(inner) }
    }
}

impl<T: Identified> Future for HidCommand<T> {
    type Item = Device<T>;
    type Error = device::Error;

    fn poll(&mut self) -> device::Result<Async<Device<T>>> {
        {
            let ref mut inner = self.inner.as_mut().expect("HidCommand polled after completion");
            inner.device.write(inner.buf.as_ref())?;
        }

        let inner = self.inner.take().unwrap();
        Ok(Async::Ready(inner.device))
    }
}

fn main() {
    let dev = device::Device::<device::MidiFader>::enumerate().unwrap().take(1).next().expect("No device found").unwrap();
    println!("Dev!");
    let cmd = HidCommand::new(dev, Report::new())
        .and_then(|d| {
            println!("Reading!");
            d.read(Report::new())
        })
        .and_then(|r| {
            println!("Got {} bytes!", r.2);
            Ok(())
        })
        .map_err(|e| {
            println!("Oh noes! {:?}", e);
        });

    tokio::run(cmd);
}
