/**
 * USB Midi-Fader
 *
 * Kevin Cuzner
 *
 * Main Application
 */

#include "usb.h"
#include "osc.h"

int main()
{
    osc_request_hsi8();

    usb_init();
    usb_enable();

    while (1)
    {
    }

    return 0;
}

