//! Host software GUI for the midi-fader

extern crate arrayvec;
extern crate byteorder;
extern crate errno;
#[macro_use]
extern crate failure;
#[macro_use]
extern crate futures;
extern crate glium;
extern crate imgui;
extern crate imgui_glium_renderer;
extern crate libc;
extern crate mio;
#[macro_use]
extern crate paste;
extern crate tokio;

#[cfg(target_os="linux")]
extern crate udev;

#[cfg(target_os="windows")]
extern crate winapi;

mod device;
mod config;
mod gui;

use std::thread;
use tokio::prelude::*;
use device::{Device, MidiFaderExtensions};

fn main() {
    let (requests_tx, requests_rx) = tokio::sync::mpsc::channel(10usize);

    let handle = thread::spawn(move || {
        let configurator = config::configure(requests_rx)
            .map_err(|e| panic!("Configuration failed. {}", e));
        tokio::run(configurator);
    });

    gui::gui_main(requests_tx);
    handle.join().unwrap();
}
