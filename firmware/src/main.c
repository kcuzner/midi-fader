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
#include "osc.h"

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

static const USBInterfaceListNode hid_interface_node = {
    .interface = &hid_interface,
    .next = NULL,
};

static const USBApplicationSetup setup = {
    .interface_list = &hid_interface_node,
};

int main()
{
    osc_request_hsi8();

    usb_init();
    usb_app_init(&setup);
    usb_enable();

    while (1)
    {
    }

    return 0;
}

