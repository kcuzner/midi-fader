//! Host software GUI for the midi-fader

#[macro_use]
extern crate error_chain;
#[macro_use]
extern crate futures;
extern crate libc;
extern crate mio;
#[macro_use]
extern crate tokio;

#[cfg(target_os="linux")]
extern crate udev;

mod device;

use tokio::prelude::*;

fn main() {
    let mut dev = device::Device::<device::MidiFader>::enumerate().unwrap().take(1).next().expect("No device found").unwrap();
    println!("Dev!");
    let mut report = device::Report::new();
    dev.poll_read(&mut report);
}
