//! Host software GUI for the midi-fader

#[macro_use]
extern crate error_chain;

#[cfg(target_os="linux")]
extern crate udev;

mod device;

fn main() {
    for res in device::MidiFaderDevice::enumerate().unwrap() {
        res.open();
    }
}
