/**
 * USB Midi Device Driver
 *
 * Kevin Cuzner
 */

#include "usb_midi.h"

#include <stdint.h>
#include <stddef.h>

#include "_gen_usb_desc.h"

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
 * As the user queues events, the are stored in a multi-buffered event buffer
 * until they are flushed out to the main USB driver (which in turn copies the
 * contents of the buffer into successive transfers until it is empty). A
 * flush is triggered in the following scenarios:
 *  - A buffer is filled.
 *  - A USB SOF is received.
 *  - The user application requests a flush.
 *
 * When a flush occurs, the ownership of that particular buffer is transferred
 * to the USB driver and won't be released back to the user application until
 * the transfer is complete. The MIDI Interface maintains several buffers to
 * reduce the impact of buffer non-availability when the user application is
 * queueing data.
 *
 * For receiving, there are also multi-buffered event buffers that are given to
 * the USB driver and released back to the MIDI Interface as the endpoint
 * receives events. When events are received, the MIDI Interface immediately
 * gives an empty buffer to the USB Driver so that it can continue receiving
 * events. The MIDI interface will then call the hook function to notify the
 * user application that events have been received.
 *
 * Other than managing the Cable Numbers, the MIDI Interface does not do any
 * processing of the events. It is the responsibilty of the user application
 * to ensure that it queues valid events.
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

/**
 * Node in the linked list used for tracking buffers
 */
typedef struct MidiBufferEntry {
    uint8_t length;
    USBMidiEvent *buffer;
    struct MidiBufferEntry *next;
} MidiBufferEntry;

static USBMidiEvent midi_buffer2[USB_MIDI_BUFFER_LEN];
static MidiBufferEntry midi_buffer_entry2 = {
    .length = 0,
    .buffer = midi_buffer2,
    .next = NULL,
};

static USBMidiEvent midi_buffer1[USB_MIDI_BUFFER_LEN];
static MidiBufferEntry midi_buffer_entry1 = {
    .length = 0,
    .buffer = midi_buffer1,
    .next = &midi_buffer_entry2,
};

static USBMidiEvent midi_buffer0[USB_MIDI_BUFFER_LEN];
static MidiBufferEntry midi_buffer_entry0 = {
    .length = 0,
    .buffer = midi_buffer0,
    .next = &midi_buffer_entry1,
};

/**
 * Head of the application buffer list
 */
static MidiBufferEntry *midi_buffer_app = &midi_buffer_entry0;
/**
 * Head of the USB buffer list
 */
static MidiBufferEntry *midi_buffer_usb = NULL;

void __attribute__((weak)) hook_usb_midi_received(const USBMidiEvent *events, uint8_t eventCount) { }
void __attribute__((weak)) hook_usb_midi_configured(void) { }

/**
 * Helper function which moves the current application buffer to the USB list
 */
static void usb_midi_next_app_buffer(void)
{
    if (!midi_buffer_app)
        return;

    // I'm about 99% sure we have a race condition here. I think I'll need to disable interrupts.
    MidiBufferEntry *new_buffer = midi_buffer_app;
    midi_buffer_app = midi_buffer_app->next;
    new_buffer->next = NULL;

    if (!midi_buffer_usb)
    {
        midi_buffer_usb = new_buffer;
    }
    else
    {
        MidiBufferEntry *current_entry = midi_buffer_usb;
        while (current_entry && current_entry->next)
            current_entry = current_entry->next;
        current_entry->next = new_buffer;
    }
}

/**
 * Helper function which moves the current usb buffer to the application list
 */
static void usb_midi_next_usb_buffer(void)
{
    if (!midi_buffer_usb)
        return;

    //Probably a race condition here
    MidiBufferEntry *new_buffer = midi_buffer_usb;
    midi_buffer_usb = midi_buffer_usb->next;
    new_buffer->next = NULL;

    if (!midi_buffer_app)
    {
        midi_buffer_app = new_buffer;
    }
    else
    {
        MidiBufferEntry *current_entry = midi_buffer_app;
        while (current_entry && current_entry->next)
            current_entry = current_entry->next;
        current_entry->next = new_buffer;
    }
}

static void usb_midi_queue_event(const USBMidiEvent *event)
{
    // Spin-wait until the app buffer becomes available
    while (!midi_buffer_app) { }

    uint8_t index = midi_buffer_app->length++;
    midi_buffer_app->buffer[index] = *event;

    if (index >= USB_MIDI_BUFFER_LEN)
    {
        usb_midi_next_app_buffer();
        //TODO: Flush
    }
}

void usb_midi_send(USBMidiCodeIndex codeIndex, const uint8_t *data, uint8_t len)
{
    //truncate data
    if (len > 3)
        len = 3;

    // create the event
    USBMidiEvent event = {
        .cableNumber = USB_MIDI_OUT_JACK_ID,
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
    usb_endpoint_setup(MIDI_IN_ENDPOINT, MIDI_IN_ENDPOINT, USB_MIDI_ENDPOINT_SIZE, USB_ENDPOINT_BULK, USB_FLAGS_NONE);
    usb_endpoint_setup(MIDI_OUT_ENDPOINT, MIDI_OUT_ENDPOINT, USB_MIDI_ENDPOINT_SIZE, USB_ENDPOINT_BULK, USB_FLAGS_NONE);

    hook_usb_midi_configured();
}

static void usb_midi_sof(void)
{
}

static void usb_midi_endpoint_sent(uint8_t endpoint, void *buf, uint16_t len)
{
}

static void usb_midi_endpoint_received(uint8_t endpoint, void *buf, uint16_t len)
{
}

const USBInterface midi_interface = {
    .hook_usb_handle_setup_request = &usb_midi_handle_setup_request,
    .hook_usb_set_configuration = &usb_midi_set_configuration,
    .hook_usb_sof = &usb_midi_sof,
    .hook_usb_endpoint_sent = &usb_midi_endpoint_sent,
    .hook_usb_endpoint_received = &usb_midi_endpoint_received,
};

