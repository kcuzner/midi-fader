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
 * <device bcdUSB="0x0200" bDeviceClass="0" bDeviceSubClass="0"
 *   bDeviceProtocol="0" bMaxPacketSize0="USB_CONTROL_ENDPOINT_SIZE"
 *   idVendor="0x16c0" idProduct="0x05dc" bcdDevice="0x0011"
 *   iManufacturer="manufacturer" iProduct="product" />
 * <string id="manufacturer" lang="0x0409">kevincuzner.com</string>
 * <string id="product" lang="0x0409">Midi-Fader</string>
 * <configuration bmAttributes="0x80" bMaxPower="250" />
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

