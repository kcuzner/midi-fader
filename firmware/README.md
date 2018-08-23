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

## What can be configured

Generally:

- Event rate (`0x80000001`)

For each button:

- Channel (`0x4000NN01, NN = button number`)
- "On" value (`0x4000NN02`)
- "Off" value (`0x4000NN03`)
- Mode: Control or Note (`0x4000NN04`)
- Control number (`0x4000NN05`)
- Note number (`0x4000NN06`)

For each fader

- Channel (`0x2000NN01`, NN = fader number)
- Mode: Control or Pitch Bend (`0x2000NN02`)
- Control number (`0x2000NN03`)
- Control minimum (`0x2000NN04`)
- Control maximum (`0x2000NN05`)
- Pitch minimum (`0x2000NN06`)
- Pitch maximum (`0x2000NN07`)

Other read-only parameters:

- Number of faders/buttons: (`0x80000002`)

### Configuration Protocol

Configuration is accomplished through the HID interface, rather than directly
through MIDI (but maybe this will come later). The communication is arranged
through the two 64-byte IN and OUT reports.

In general, the reports have the following format:

```
Bytes 0-3: Command number
Bytes 4-7: Parameter 0
Bytes 8-11: Parameter 1
...
```

Words should be packed LSB first.

Every OUT report will be 

An IN report will contain the command number of the command it is responding to.
Parameters can be IN, OUT, or both. The parameters will be echoed in the
corresponding OUT report.

#### Status

The Status command has the following format:

 - Command: 0x00
 - Parameter 0 (IN): Constant 0xDEADBEEF
 - Remaining parameters (IN): Null terminated git version string of the firmware

#### Set Parameter

The Set Parameter command has the following format:

 - Command: 0x80
 - Parameter 0 (IN): Status (0 = success, error code otherwise)
 - Parameter 1 (OUT): Parameter number
 - Parameter 2 (OUT): Parameter value

#### Get Parameter

The Get Parameter command has the following format:

 - Command: 0x40
 - Parameter 0 (IN): Status (0 = success, error code otherwise)
 - Parameter 1 (OUT): Parameter number
 - Parameter 2 (IN): Parameter value

