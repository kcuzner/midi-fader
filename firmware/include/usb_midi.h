/**
 * USB Midi Audio Device driver
 *
 * Hooks into the USB core driver and provides MIDI Jacks to an element
 * controlled by the user application.
 *
 * Kevin Cuzner
 */

#ifndef _USB_MIDI_H_
#define _USB_MIDI_H_

#include "usb.h"
#include "usb_app.h"

#define USB_MIDI_ENDPOINT_SIZE 64
#define USB_MIDI_IN_JACK_ID 0x01
#define USB_MIDI_OUT_JACK_ID 0x02
#define USB_MIDI_ELEMENT_ID 0x03

typedef enum {
    MIDI_MISC_FN = 0x00,
    MIDI_CABLE_EV = 0x01,
    MIDI_SC2 = 0x02, //2-byte system common messages
    MIDI_SC3 = 0x03, //3-byte system common messages
    MIDI_SYSEX_START = 0x04, //sysex starts or continues
    MIDI_SYSEX1_SC1 = 0x05, //1-byte system common message or sysex ends with single byte
    MIDI_SYSEX2 = 0x06, //sysex ends with 2 bytes
    MIDI_SYSEX3 = 0x07, //sysex ends with 3 byets
    MIDI_NOTE_ON = 0x08, //note on event
    MIDI_NOTE_OFF = 0x09, //note off event
    MIDI_KEYPRESS = 0x0A, //poly-keypress
    MIDI_CTRL = 0x0B, //control change
    MIDI_PRG = 0x0C, //program change
    MIDI_CH_PRESSURE = 0x0D, //channel pressure
    MIDI_PITCH_BEND = 0x0E, //pitch bend change
    MIDI_BYTE = 0x0F, //single byte
} USBMidiCodeIndex;

/**
 * Represents a USB midi event
 *
 * Horribly type-punned for ease of use
 */
typedef union {
    uint32_t contents;
    uint8_t bytes[4];
    struct {
        union {
            uint8_t byte0;
            struct {
                unsigned cableNumber:4;
                unsigned codeIndex:4;
            };
        };
        uint8_t byte1;
        uint8_t byte2;
        uint8_t byte3;
    };
} __attribute__((packed)) USBMidiEvent;

/**
 * Queues a midi event for sending
 *
 * This will spin-block if all buffers are full.
 *
 * codeIndex: Code index to send
 * data: Pointer to a data array that will be sent
 * len: Length of the data array, maximum 3, minimum 0
 */
void usb_midi_send(USBMidiCodeIndex codeIndex, const uint8_t *data, uint8_t len);

/**
 * Flushes the send event queue
 *
 * The queue is flushed automatically when a buffer is full and when a USB
 * SOF is received. This method allows for the application to explicitly
 * request a flush.
 */
void usb_midi_flush(void);

/**
 * Hook function called when the midi function is configured
 */
void hook_usb_midi_configured(void);

/**
 * Hook function called when midi events are received
 */
void hook_usb_midi_received(const USBMidiEvent *events, uint8_t eventCount);

/**
 * USB interface object for the app
 */
extern const USBInterface midi_interface;

#endif //_USB_MIDI_H_

