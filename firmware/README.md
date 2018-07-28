# USB Midi-Fader Firmware

This is the firmware for the USB Midi-Fader. It runs on an STM32F042X6
microcontroller and has the following features:

- Enumerates as a USB Human Interface Device
- Enumerates as a USB Audio Device (WIP)
- Reads up to 8 attached shift registers for button inputs (WIP)
  - Detects how many shift registers are attached (WIP)
- Writes up to 8 attached shift registers for LED outputs (WIP)
- Reads up to 8 analog inputs (WIP)

## Prerequisites

- `arm-none-eabi-gcc`
- `arm-none-eabi-binutils`
- `openocd`

## Build process

```
$ make
```

## Install process

```
$ make install
```

