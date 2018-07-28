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

