/**
 * USB Midi Device Driver
 *
 * Kevin Cuzner
 */

#include "usb_midi.h"

#include <stdint.h>
#include <stddef.h>
#include <assert.h>


#include "_gen_usb_desc.h"
#include "stm32f0xx.h"

/**
 * Theory of Operation
 *
 *
 * The USB MIDI interface presents a standard MIDISTREAMING USB interface to
 * the host PC. It contains the following
 *  - One Element which represents the user application to the host PC
 *  - One embedded IN Jack, which is routed into the Element
 *  - One embedded OUT Jack, which is routed out of the Element
 *
 * The user application interacts with the USB MIDI interface by way of two
 * methods:
 *  - usb_midi_send: Queues a single USB MIDI Event for sending
 *  - hook_usb_midi_received: Handles reception of a block of midi events.
 *
 * As the user queues events, the are stored in an event buffer until they are
 * flushed out to the main USB driver (which in turn copies the contents of the
 * buffer into successive transfers until it is empty). A flush is triggered in
 * the following scenarios:
 *  - A buffer is filled.
 *  - A USB SOF is received.
 *  - The user application requests a flush.
 *
 * When a flush occurs, the ownership of that particular buffer is transferred
 * to the USB driver and won't be released back to the user application until
 * the transfer is complete.
 *
 * For receiving, there is only one buffer. After the hook_midi_usb_received
 * function terminates, it is released back to the USB driver.
 *
 * Other than managing the Cable Numbers, the MIDI Interface does not do any
 * processing of the events. It is the responsibilty of the user application to
 * ensure that it queues valid events.
 */

/**
 * <include>usb_midi.h</include>
 * <descriptor id="audio_control" type="0x04" childof="configuration">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <index name="bInterfaceNumber" size="1" />
 *  <byte name="bAlternateSetting">0</byte>
 *  <count name="bNumEndpoints" type="0x05" associated="associated" size="1" />
 *  <byte name="bInterfaceClass">0x01</byte><!-- AUDIO -->
 *  <byte name="bInterfaceSubClass">0x01</byte><!-- AUDIO_CONTROL -->
 *  <byte name="bInterfaceProtocol">0x00</byte>
 *  <byte name="iInterface">0</byte>
 *  <children type="0x24" />
 * </descriptor>
 * <descriptor id="uac_header" type="0x24" childof="audio_control">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <byte name="bDescriptorSubtype">0x01</byte><!-- HEADER subtype -->
 *  <word name="bcdADC">0x0100</word>
 *  <length name="wTotalLength" size="2" all="all" />
 *  <byte name="bInCollection">1</byte>
 *  <ref name="baInterfaceNr" type="0x04" refid="midi_streaming" size="1" />
 * </descriptor>
 * <descriptor id="midi_streaming" type="0x04" childof="configuration">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <index name="bInterfaceNumber" size="1" />
 *  <byte name="bAlterateSetting">0</byte>
 *  <count name="bNumEndpoints" type="0x05" associated="associated" size="1" />
 *  <byte name="bInterfaceClass">0x01</byte><!-- AUDIO -->
 *  <byte name="bInterfaceSubclass">0x03</byte><!-- MIDISTREAMING -->
 *  <byte name="bInterfaceProtocol">0x00</byte>
 *  <byte name="iInterface">0</byte>
 *  <children type="0x24" />
 *  <children type="0x05" />
 * </descriptor>
 * <descriptor id="ms_interface" type="0x24" childof="midi_streaming">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <byte name="bDescriptorSubtype">0x01</byte><!-- MS_HEADER subtype -->
 *  <word name="bcdADC">0x0100</word>
 *  <length name="wTotalLength" size="2" all="all" />
 *  <children type="0x24" />
 * </descriptor>
 * <descriptor id="midi_in_jack" type="0x24" childof="ms_interface">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <byte name="bDescriptorSubtype">0x02</byte><!-- MIDI_IN_JACK subtype -->
 *  <byte name="bJackType">0x01</byte><!-- EMBEDDED -->
 *  <byte name="bJackId">USB_MIDI_IN_JACK_ID</byte>
 *  <byte name="iJack">0</byte>
 * </descriptor>
 * <descriptor id="midi_out_jack" type="0x24" childof="ms_interface">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <byte name="bDescriptorSubtype">0x03</byte><!-- MIDI_OUT_JACK subtype -->
 *  <byte name="bJackType">0x01</byte><!-- EMBEDDED -->
 *  <byte name="bJackId">USB_MIDI_OUT_JACK_ID</byte>
 *  <byte name="bNrInputPins">1</byte>
 *  <byte name="BaSourceID1">USB_MIDI_ELEMENT_ID</byte>
 *  <byte name="BaSourcePin1">1</byte>
 *  <byte name="iJack">0</byte>
 * </descriptor>
 * <descriptor id="midi_element" type="0x24" childof="ms_interface">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <byte name="bDescriptorSubtype">0x04</byte><!-- ELEMENT subtype -->
 *  <byte name="bElementId">USB_MIDI_ELEMENT_ID</byte>
 *  <byte name="bNrInputPins">1</byte>
 *  <byte name="baSourceId1">USB_MIDI_IN_JACK_ID</byte>
 *  <byte name="baSourcePin1">1</byte>
 *  <byte name="bNrOutputPins">1</byte>
 *  <byte name="bInTerminalLink">0</byte>
 *  <byte name="bOutTerminalLink">0</byte>
 *  <byte name="bElCapsSize">1</byte>
 *  <byte name="bmElementCaps">0x01</byte>
 *  <byte name="iElement">0</byte>
 * </descriptor>
 * <descriptor id="midi_out_endpoint" type="0x05" childof="midi_streaming">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <outendpoint name="bEndpointAddress" define="MIDI_OUT_ENDPOINT" />
 *  <byte name="bmAttributes">0x02</byte>
 *  <word name="wMaxPacketSize">USB_MIDI_ENDPOINT_SIZE</word>
 *  <byte name="bInterval">0</byte>
 *  <byte name="bRefresh">0</byte>
 *  <byte name="bSynchAddress">0</byte>
 *  <children type="0x25" />
 * </descriptor>
 * <descriptor id="midi_out_endpoint_jack" type="0x25" childof="midi_out_endpoint">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <byte name="bDescriptorSubtype">0x01</byte><!-- MS_GENERAL subtype -->
 *  <byte name="bNumEmbMIDIJack">1</byte>
 *  <byte name="baAssocJackId1">USB_MIDI_IN_JACK_ID</byte>
 * </descriptor>
 * <descriptor id="midi_in_endpoint" type="0x05" childof="midi_streaming">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <inendpoint name="bEndpointAddress" define="MIDI_IN_ENDPOINT" />
 *  <byte name="bmAttributes">0x02</byte>
 *  <word name="wMaxPAcketSize">USB_MIDI_ENDPOINT_SIZE</word>
 *  <byte name="bInterval">0</byte>
 *  <byte name="bRefresh">0</byte>
 *  <byte name="bSynchAddress">0</byte>
 *  <children type="0x25" />
 * </descriptor>
 * <descriptor id="midi_in_endpoint_jack" type="0x25" childof="midi_in_endpoint">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <byte name="bDescriptorSubtype">0x01</byte><!-- MS_GENERAL subtype -->
 *  <byte name="bNumEmbMIDIJack">1</byte>
 *  <byte name="baAssocJackId1">USB_MIDI_OUT_JACK_ID</byte>
 * </descriptor>
 */

#define USB_MIDI_BUFFER_LEN 16

static_assert(sizeof(USBMidiEvent) == 4, "USBMidiEvent must be 4 bytes");

typedef enum { MIDI_BUF_ACTIVE, MIDI_BUF_QUEUED, MIDI_BUF_SENDING } MidiBufStatus;

/**
 * Node in the linked list used for tracking buffers
 */
typedef struct MidiBufferEntry {
    uint8_t index;
    USBMidiEvent *buffer;
    MidiBufStatus status;
    struct MidiBufferEntry *next;
} MidiBufferEntry;


static USB_DATA_ALIGN USBMidiEvent midi_transmit_buf[USB_MIDI_BUFFER_LEN];
static const MidiBufferEntry midi_transmit_buf_entry_default = {
    .index = 0,
    .buffer = midi_transmit_buf,
    .status = MIDI_BUF_ACTIVE,
    .next = NULL,
};
static volatile MidiBufferEntry midi_transmit_buf_entry = midi_transmit_buf_entry_default;
/**
 * Receive buffer. No special stuff here.
 */
static USBMidiEvent midi_receive_buf[USB_MIDI_BUFFER_LEN];

void __attribute__((weak)) hook_usb_midi_received(const USBMidiEvent *events, uint8_t eventCount) { }
void __attribute__((weak)) hook_usb_midi_configured(void) { }

/**
 * Helper function which moves the current application buffer to the USB list
 */
static void usb_midi_next_app_buffer(void)
{
    // if the buffer is not active, don't change its state
    if (midi_transmit_buf_entry.status != MIDI_BUF_ACTIVE)
        return;

    // Avoid race conditions. This function could be interrupted by the corresponding
    // usb buffer function.
    __disable_irq();

    // Queue the buffer to be sent on the next flush
    midi_transmit_buf_entry.status = MIDI_BUF_QUEUED;

    __enable_irq();
}

/**
 * Flushes the next buffer to be sent, if able
 */
void usb_midi_flush(void)
{
    // Are we already sending a buffer? Or is there no buffer to send?
    if (midi_transmit_buf_entry.status != MIDI_BUF_QUEUED)
        return;

    // Avoid a race condition
    __disable_irq();

    midi_transmit_buf_entry.status = MIDI_BUF_SENDING;

    usb_endpoint_send(MIDI_IN_ENDPOINT, midi_transmit_buf_entry.buffer, midi_transmit_buf_entry.index * sizeof(USBMidiEvent));

    __enable_irq();

}

static void usb_midi_queue_event(const USBMidiEvent *event)
{
    // Spin-wait until the app buffer becomes available
    while (midi_transmit_buf_entry.status != MIDI_BUF_ACTIVE) { }

    uint8_t index = midi_transmit_buf_entry.index++;
    midi_transmit_buf_entry.buffer[index] = *event;

    if (midi_transmit_buf_entry.index >= USB_MIDI_BUFFER_LEN)
    {
        usb_midi_next_app_buffer();
        usb_midi_flush();
    }
}

void usb_midi_send(USBMidiCodeIndex codeIndex, const uint8_t *data, uint8_t len)
{
    //truncate data
    if (len > 3)
        len = 3;

    // create the event
    USBMidiEvent event = {
        .cableNumber = 0,
        .codeIndex = codeIndex,
    };
    for (uint8_t i = 0; i < len; i++)
    {
        event.bytes[i+1] = data[i];
    }

    usb_midi_queue_event(&event);
}

static USBAppControlResult usb_midi_handle_setup_request(USBSetupPacket const *setup, USBTransferData *nextTransfer)
{
    return USB_APP_CTL_UNHANDLED;
}

static void usb_midi_set_configuration(uint16_t configuration)
{
    usb_endpoint_setup(MIDI_IN_ENDPOINT, 0x80 | MIDI_IN_ENDPOINT, USB_MIDI_ENDPOINT_SIZE, USB_ENDPOINT_BULK, USB_FLAGS_NONE);
    usb_endpoint_setup(MIDI_OUT_ENDPOINT, 0x00 | MIDI_OUT_ENDPOINT, USB_MIDI_ENDPOINT_SIZE, USB_ENDPOINT_BULK, USB_FLAGS_NONE);

    midi_transmit_buf_entry = midi_transmit_buf_entry_default;

    hook_usb_midi_configured();

    //usb_endpoint_receive(MIDI_OUT_ENDPOINT, midi_receive_buf, sizeof(midi_receive_buf));
}

static void usb_midi_sof(void)
{
    usb_midi_next_app_buffer();
    usb_midi_flush();
}

static void usb_midi_endpoint_sent(uint8_t endpoint, void *buf, uint16_t len)
{
    if (endpoint == MIDI_IN_ENDPOINT)
    {
        __disable_irq();

        //give the buffer back to the application
        midi_transmit_buf_entry.index = 0;
        midi_transmit_buf_entry.status = MIDI_BUF_ACTIVE;

        __enable_irq();

        usb_midi_flush();
    }
}

static void usb_midi_endpoint_received(uint8_t endpoint, void *buf, uint16_t len)
{
    if (endpoint == MIDI_OUT_ENDPOINT)
    {
        uint8_t count = len / sizeof(USBMidiEvent);
        hook_usb_midi_received((USBMidiEvent *)buf, count);

        usb_endpoint_receive(MIDI_OUT_ENDPOINT, midi_receive_buf, sizeof(midi_receive_buf));
    }
}

const USBInterface midi_interface = {
    .hook_usb_handle_setup_request = &usb_midi_handle_setup_request,
    .hook_usb_set_configuration = &usb_midi_set_configuration,
    .hook_usb_sof = &usb_midi_sof,
    .hook_usb_endpoint_sent = &usb_midi_endpoint_sent,
    .hook_usb_endpoint_received = &usb_midi_endpoint_received,
};

