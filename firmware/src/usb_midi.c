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
#include "atomic.h"

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
 * As the application queues events, they are placed in a ring buffer. When the
 * USB peripheral can accept more events to send, they are queued from the
 * buffer. If the user application queues events faster than they can be
 * flushed to the USB, there are two options:
 *  - Pass a flag indicating that it is ok to overwrite the oldest event with
 *    the passed event.
 *  - Wait until buffer space becomes available.
 *
 * When the USB Peripheral completes sending a buffer and there is data to send
 * or if a SOF occurs, the current portion of the ring buffer filled with
 * events is copied into an intermediate buffer which will be used by the USB
 * peripheral during transmission, even if there are zero events to send. The
 * reason for doing things this way is to try and prevent stale data from
 * accumulating in the USB peripheral. From what I can tell, it is not possible
 * to "cancel" a pending transmit without possibly entering some weird state on
 * the USB peripheral. This means that once data is sent to the peripheral, it
 * is committed until it is actually sent, which could be quite a long time.
 * Since this can result in (possibly very) stale data being delivered to the
 * user, we try to minimize that chance by queuing up ZLPs which are harmless
 * if they take forever to deliver. Once a ZLP is finally sent, the active data
 * (if there is any) is then transferred to the peripheral. There is still a
 * chance that this active data could remain in the buffer for some time and
 * become stale, but to actually have that happen would require the user
 * shutting down the host's MIDI reader while simultaneously causing events to
 * be created by manipulating the device so that an IN following the reading of
 * a ZLP is not read.
 *
 * This system does rely on the application not continuously generating data.
 * If the application does continuously generate data, then stale data could be
 * trapped in the buffer because there wasn't a chance to send a ZLP.
 *
 * For receiving, there is only one buffer. After the hook_midi_usb_received
 * function terminates, it is released back to the USB driver.
 *
 * Other than managing the Cable Numbers, the MIDI Interface does not do any
 * validation of the events. It is the responsibilty of the user application to
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

static_assert(sizeof(USBMidiEvent) == 4, "USBMidiEvent must be 4 bytes");
static_assert(USB_MIDI_TX_QUEUE_SIZE > 0, "Transmit queue size must be nonzero");
static_assert(USB_MIDI_RX_QUEUE_SIZE > 0, "Receive queue size must be nonzero");

typedef enum { MIDI_BUF_ACTIVE, MIDI_BUF_QUEUED, MIDI_BUF_SENDING } MidiBufStatus;

/**
 * Event buffer for events being queued
 */
static USBMidiEvent midi_send_event_queue[USB_MIDI_TX_QUEUE_SIZE];

/**
 * Index in the send event buffer where the first unsent event lives
 *
 * This should only be modified by the main thread of execution, not during
 * an interrupt.
 */
static uint32_t midi_send_event_start_idx;

/**
 * Index in the send event buffer where the next event will be queued
 */
static uint32_t midi_send_event_end_idx;

/**
 * Intermediate buffer for use by the USB peripheral when sending data
 */
static USBMidiEvent midi_send_usb_buf[USB_MIDI_TX_QUEUE_SIZE];

/**
 * Holds the status of the USB peripheral for this module
 */
static struct {
    unsigned configured:1;
    unsigned sending:1;
} midi_usb_status;

/**
 * Receive buffer. No special stuff here.
 */
static USBMidiEvent midi_receive_buf[USB_MIDI_RX_QUEUE_SIZE];

void __attribute__((weak)) hook_usb_midi_received(const USBMidiEvent *events, uint8_t eventCount) { }
void __attribute__((weak)) hook_usb_midi_configured(void) { }
void __attribute__((weak)) hook_usb_midi_send_complete(void) { }

#define USB_MIDI_NEXT_TX_IDX(C) (((C) + 1) % USB_MIDI_TX_QUEUE_SIZE)

/**
 * Waits for a space in the send buffer to appear
 *
 * This should only be called while queuing a new event.
 */
static void usb_midi_send_buf_wait()
{
    // Justification for operation outside a critical section:
    //
    // midi_send_event_end_idx is modified only in the thread of execution that
    // calls this function. This means that it won't change. However,
    // midi_send_event_start_idx may increase while this function is running.
    // The value that will be seen is either equal if the buffer is still full
    // or not equal (and possibly some intermediate non-equal) value which
    // indicates that there will be at least one spot opening in the buffer.

    while (USB_MIDI_NEXT_TX_IDX(midi_send_event_end_idx) ==
            midi_send_event_start_idx) { }
}

static void usb_midi_queue_event(const USBMidiEvent *event, USBMidiSendType sendType)
{
    // Spin-wait until the app buffer becomes available
    if (sendType == USB_MIDI_BLOCK)
        usb_midi_send_buf_wait();

    // Queue the event, advancing the index
    //
    // This requires a critical section
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        midi_send_event_queue[midi_send_event_end_idx] = *event;
        midi_send_event_end_idx = USB_MIDI_NEXT_TX_IDX(midi_send_event_end_idx);
        if (midi_send_event_end_idx == midi_send_event_start_idx)
        {
            // We just lost an event due to the buffer being full.
            // Advance the start index.
            midi_send_event_start_idx = USB_MIDI_NEXT_TX_IDX(midi_send_event_start_idx);
        }
    }
}

void usb_midi_send(USBMidiCodeIndex codeIndex, const uint8_t *data, uint8_t len, USBMidiSendType sendType)
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

    usb_midi_queue_event(&event, sendType);
}

/**
 * Sends the contents of the queue to the USB peripheral, even if its empty.
 */
static void usb_midi_send_queue(void)
{
    if (midi_usb_status.sending || !midi_usb_status.configured)
        return;

    midi_usb_status.sending = 1;
    uint32_t len = 0;
    // Justification for no atomic section: This is either called from within
    // an atomic section or during the USB ISR (in which case
    // midi_send_event_end_idx will not move)
    for (; midi_send_event_start_idx != midi_send_event_end_idx;
            midi_send_event_start_idx = USB_MIDI_NEXT_TX_IDX(midi_send_event_start_idx))
    {
        midi_send_usb_buf[len++] = midi_send_event_queue[midi_send_event_start_idx];
    }

    usb_endpoint_send(MIDI_IN_ENDPOINT, midi_send_usb_buf, len * sizeof(USBMidiEvent));
}

void usb_midi_flush(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (midi_send_event_start_idx == midi_send_event_end_idx)
            return;

        usb_midi_send_queue();
    }
}

static void usb_midi_reset(void)
{
    midi_usb_status.sending = 0;
    midi_usb_status.configured = 0;
}

static USBAppControlResult usb_midi_handle_setup_request(USBSetupPacket const *setup, USBTransferData *nextTransfer)
{
    return USB_APP_CTL_UNHANDLED;
}

static void usb_midi_set_configuration(uint16_t configuration)
{
    usb_endpoint_setup(MIDI_IN_ENDPOINT, 0x80 | MIDI_IN_ENDPOINT, USB_MIDI_ENDPOINT_SIZE, USB_ENDPOINT_BULK, USB_FLAGS_NONE);
    usb_endpoint_setup(MIDI_OUT_ENDPOINT, 0x00 | MIDI_OUT_ENDPOINT, USB_MIDI_ENDPOINT_SIZE, USB_ENDPOINT_BULK, USB_FLAGS_NONE);

    midi_send_event_start_idx = midi_send_event_end_idx = 0;
    midi_usb_status.configured = 1;

    // Queue a ZLP
    usb_midi_send_queue();

    hook_usb_midi_configured();

    usb_endpoint_receive(MIDI_OUT_ENDPOINT, midi_receive_buf, sizeof(midi_receive_buf));
}

static void usb_midi_sof(void)
{
    static uint32_t count = 0;

    ++count;
    if (count < USB_MIDI_TX_INTERVAL_MS)
        return;

    count = 0;
    usb_midi_send_queue();
}

static void usb_midi_endpoint_sent(uint8_t endpoint, void *buf, uint16_t len)
{
    if (endpoint == MIDI_IN_ENDPOINT)
    {
        // It is here that we might cause data to get stuck in the USB Peripheral
        // and go stale.
        midi_usb_status.sending = 0;
        hook_usb_midi_send_complete();
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
    .hook_usb_reset = &usb_midi_reset,
    .hook_usb_handle_setup_request = &usb_midi_handle_setup_request,
    .hook_usb_set_configuration = &usb_midi_set_configuration,
    .hook_usb_sof = &usb_midi_sof,
    .hook_usb_endpoint_sent = &usb_midi_endpoint_sent,
    .hook_usb_endpoint_received = &usb_midi_endpoint_received,
};

