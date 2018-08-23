# USB Midi-Fader Firmware

This is the firmware for the USB Midi-Fader. It runs on an STM32F042X6
microcontroller and has the following features:

- Enumerates as a USB Human Interface Device
- Enumerates as a USB Audio Device
- Reads up to 8 attached shift registers for button inputs (WIP)
  - Detects how many shift registers are attached (WIP)
- Writes up to 8 attached shift registers for LED outputs (WIP)
- Reads up to 8 analog inputs (WIP)
- Completely configurable in terms of what it sends (WIP)

## Build Prerequisites

These are names of Arch Linux packages.

- `arm-none-eabi-gcc`
- `arm-none-eabi-binutils`
- `python` (Python 3)
- `openocd`

## Build process

```
$ make
```

## Install process

```
$ make install
```

