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
use device::{Device, MidiFaderCommandArgs, MidiFaderCommand};

fn main() {
    let dev = Device::<device::MidiFader>::enumerate().unwrap().take(1).next().expect("No device found").unwrap();
    println!("Dev!");
    let args = MidiFaderCommandArgs::new();
    let cmd = MidiFaderCommand::new(dev, args)
        .and_then(|r| {
            println!("Got {:?}", r.1);
            Ok(())
        })
        .map_err(|e| {
            println!("Oh noes! {:?}", e);
        });

    tokio::run(cmd);
}
