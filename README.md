# Simple MIDI Fader

By Kevin Cuzner

## Project Status

 * PCB complete and working (with an additional load added to reduce regulator noise)
 * Firmware feature-complete and mostly working
   * Emulates as HID and USB-MIDI devices
   * Not tested on Windows yet
 * Case OpenSCAD in progress
 * Configuration host software in progress

## Featureset

 * Two 60mm analog fader inputs
 * USB Audio Class interface, aiming for driverless on Windows 7 and 10
 * Driverless HID interface for configuration
   * It might be trivially simple to echo the fader status in the HID reports...

## Main Component Options

 * Faders (note that they have different footprints and different levers):
   * Expensive faders ($4.74, very light movement force): PTB6053-2010APB103-ND
   * Cheap faders ($1.73): PTA6043-2015CPB103-ND
 * Microcontrollers:
   * 497-17354-1-ND: STM32F070F6P6, $1.48, 32K flash, 6K RAM, 20-TSSOP.
     * No CRS, external XTAL required.
   * 497-15099-ND: STM32F070CBT6, $2.52, 128K flash, 16K RAM, 48-LQFP
     * No CRS, exteranl XTAL required.
   * 497-17344-ND: STM32042F6P6, $2.25, 32K flash, 6K RAM, 20-TSSOP.
     * Has a CRS
     * 24.6mA with all peripherals and HSI48 enabled
 * 609-4052-1-ND: Micro-USB, $0.76
 * 455-1790-1-ND: JST-SH header for programming, $0.63
 * Power supply solution:
   * MCP1603T-330I/OSCT-ND: Cheap buck regulator, but not too cheap. $1.01,
     TSOT-23
     * Noise issues at low currents (leaves PWM mode and skips pulses).
   * 576-4764-1-ND: MIC5504 buck regulator, $0.11, SOT23-5.
     * At a 100mA load, the case will heat 42.6C. Not over the top, but really
       quite hot.
     * We could do this AND the buck regulator, but just have this for the
       analog.

## Notes

### Notes about the logical MIDI protocol

 * MIDI has two types of bytes STATUS and DATA.
 * A STATUS byte has its MSB set
 * A DATA byte has its MSB cleared (hence why there are only ever 0 to 127 data
   values)
 * Usually, the lower nybble of the STATUS byte is a channel number (there are
   16 channels).
 * The number of data bytes to follow varies by which STATUS byte preceded them.
 * Interesting STATUS commands (upper nybble shown) for this project:
   * 0xB: Control Change. Followed by a controller number (1 data byte) and a
     controller value (1 data byte). Although there are possibly 127 controller
     numbers, numbers 120-127 are reserved for "Channel Mode Messages". Several
     of these are commonly used.
   * 0xE: Pitch Bend. Followed by two DATA bytes encoding a 14-bit value, LSB
     first. 0x2000 is center.
 * https://www.cs.cmu.edu/~music/cmsip/readings/MIDI%20tutorial%20for%20programmers.html
 * https://www.midi.org/specifications-old/item/table-1-summary-of-midi-message

### Notes about USB and MIDI

 * Windows XP and above seem to support USB MIDI except Elements.
   * "Elements" are things that process MIDI
   * "Jacks" are supported, and they are exactly what they sound like. This will
     implement a Jack.
   * Elements and Jacks are "Entities"
 * http://www.usb.org/developers/docs/devclass_docs/midi10.pdf


