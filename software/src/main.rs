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

fn main() {
    let mut dev = device::Device::<device::MidiFader>::enumerate().unwrap().take(1).next().expect("No device found").unwrap();
    println!("Dev!");
    let mut report = device::Report::new();
    let read = dev.read(&mut report);
}
