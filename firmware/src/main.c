/**
 * USB Midi-Fader
 *
 * Kevin Cuzner
 *
 * Main Application
 */

#include "usb.h"
#include "usb_app.h"
#include "usb_hid.h"
#include "usb_midi.h"
#include "osc.h"
#include "storage.h"

#include "_gen_usb_desc.h"

/**
 * <descriptor id="device" type="0x01">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <word name="bcdUSB">0x0200</word>
 *  <byte name="bDeviceClass">0</byte>
 *  <byte name="bDeviceSubClass">0</byte>
 *  <byte name="bDeviceProtocol">0</byte>
 *  <byte name="bMaxPacketSize0">USB_CONTROL_ENDPOINT_SIZE</byte>
 *  <word name="idVendor">0x16c0</word>
 *  <word name="idProduct">0x05dc</word>
 *  <word name="bcdDevice">0x0010</word>
 *  <ref name="iManufacturer" type="0x03" refid="manufacturer" size="1" />
 *  <ref name="iProduct" type="0x03" refid="product" size="1" />
 *  <byte name="iSerialNumber">0</byte>
 *  <count name="bNumConfigurations" type="0x02" size="1" />
 * </descriptor>
 * <descriptor id="lang" type="0x03" first="first">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <foreach type="0x03">
 *    <echo name="wLang" />
 *  </foreach>
 * </descriptor>
 * <descriptor id="manufacturer" type="0x03" wIndex="0x0409">
 *  <hidden name="wLang" size="2">0x0409</hidden>
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <string name="wString">kevincuzner.com</string>
 * </descriptor>
 * <descriptor id="product" type="0x03" wIndex="0x0409">
 *  <hidden name="wLang" size="2">0x0409</hidden>
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <string name="wString">Midi-Fader</string>
 * </descriptor>
 * <descriptor id="configuration" type="0x02">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <length name="wTotalLength" size="2" all="all" />
 *  <count name="bNumInterfaces" type="0x04" associated="associated" size="1" />
 *  <byte name="bConfigurationValue">1</byte>
 *  <byte name="iConfiguration">0</byte>
 *  <byte name="bmAttributes">0x80</byte>
 *  <byte name="bMaxPower">250</byte>
 *  <children type="0x04" />
 * </descriptor>
 */

#include <stddef.h>

static const USBInterfaceListNode midi_interface_node = {
    .interface = &midi_interface,
    .next = NULL,
};

static const USBInterfaceListNode hid_interface_node = {
    .interface = &hid_interface,
    .next = &midi_interface_node,
};

const USBApplicationSetup setup = {
    .interface_list = &hid_interface_node,
};

const USBApplicationSetup *usb_app_setup = &setup;

int main()
{
    osc_request_hsi8();

    usb_init();

    // Small delay to force a USB reset
    // TODO: Make this timed
    usb_disable();
    for (uint32_t i = 0; i < 0xFFF; i++) { }
    usb_enable();

    uint8_t control[] = {
        0xB0,
        7,
        64,
    };

    uint8_t buf[16];

    while (1)
    {
        for (uint32_t i = 0; i < 0xFFF; i++) { }

        control[2]++;
        control[2] &= 0x7F;

        usb_midi_send(MIDI_CTRL, control, sizeof(control));
        storage_read(0x8000001, buf, sizeof(buf));
    }

    return 0;
}

