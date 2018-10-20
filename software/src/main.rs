//! Host software GUI for the midi-fader

extern crate byteorder;
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
use device::{Device, MidiFaderExtensions};

fn main() {
    let dev = Device::<device::MidiFader>::enumerate().unwrap().take(1).next().expect("No device found").unwrap();
    println!("Dev!");
    let cmd = dev.get_parameter(0x4006).
        and_then(|r| {
            let v: i32 = r.1.into();
            println!("Got {}", v);
            r.0.device_status()
        }).
        and_then(|r| {
            println!("Firmware version: {}", r.1.version());
            Ok(())
        }).
        map_err(|e| {
            println!("Oh noes! {:?}", e);
        });

    tokio::run(cmd);
}
