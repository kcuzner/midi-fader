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
#include "error.h"
#include "storage.h"
#include "fader.h"
#include "buttons.h"
#include "systick.h"
#include "mackie.h"

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

static volatile uint32_t tick_count = 0;
static void update_tick(void)
{
    tick_count++;
}

uint8_t buf[16];
int main()
{
    osc_request_hsi8();

    systick_init();

    usb_init();
    fader_init();
    buttons_init();
    mackie_init();


    // Small delay to force a USB reset
    usb_disable();
    tick_count = 0;
    systick_subscribe(&update_tick);
    while (tick_count < 20) { } //at least 10ms needed, so we do 20ms
    usb_enable();

    while (1)
    {
        mackie_tick();
    }

    return 0;
}

