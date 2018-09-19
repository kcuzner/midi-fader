/**
 * STM32 USB Peripheral Driver
 *
 * USB Application Layer
 *
 * Kevin Cuzner
 */

#include "usb_app.h"

#include <stddef.h>

/**
 * The interface layer itself is quite simple, all it is doing is
 * dispatching handlers for now.
 */

#define FOREACH_INTERFACE(NODE)\
    for (const USBInterfaceListNode *NODE = usb_app_setup->interface_list; NODE; NODE = NODE->next)

__attribute__((weak)) const USBApplicationSetup *usb_app_setup = NULL;

USBControlResult hook_usb_handle_setup_request(USBSetupPacket const *setup, USBTransferData *nextTransfer)
{
    if (!usb_app_setup)
        return USB_CTL_STALL;

    USBAppControlResult res;
    FOREACH_INTERFACE(node)
    {
        if (!node->interface->hook_usb_handle_setup_request)
            continue;

        res = (*node->interface->hook_usb_handle_setup_request)(setup, nextTransfer);
        if (res != USB_APP_CTL_UNHANDLED)
            return res;
    }

    return USB_CTL_STALL;
}

void hook_usb_control_complete(USBSetupPacket const *setup)
{
    if (!usb_app_setup)
        return;

    FOREACH_INTERFACE(node)
    {
        if (!node->interface->hook_usb_control_complete)
            continue;

        (*node->interface->hook_usb_control_complete)(setup);
    }
}

void hook_usb_reset(void)
{
    if (!usb_app_setup)
        return;

    if (usb_app_setup->hook_usb_reset)
        (*usb_app_setup->hook_usb_reset)();

    FOREACH_INTERFACE(node)
    {
        if (!node->interface->hook_usb_reset)
            continue;

        (*node->interface->hook_usb_reset)();
    }
}

void hook_usb_sof(void)
{
    if (!usb_app_setup)
        return;

    if (usb_app_setup->hook_usb_sof)
        (*usb_app_setup->hook_usb_sof)();

    FOREACH_INTERFACE(node)
    {
        if (!node->interface->hook_usb_sof)
            continue;

        (*node->interface->hook_usb_sof)();
    }
}

void hook_usb_set_configuration(uint16_t configuration)
{
    if (!usb_app_setup)
        return;

    FOREACH_INTERFACE(node)
    {
        if (!node->interface->hook_usb_set_configuration)
            continue;

        (*node->interface->hook_usb_set_configuration)(configuration);
    }
}

void hook_usb_set_interface(uint16_t interface)
{
    if (!usb_app_setup)
        return;

    FOREACH_INTERFACE(node)
    {
        if (!node->interface->hook_usb_set_interface)
            continue;

        (*node->interface->hook_usb_set_interface)(interface);
    }
}

void hook_usb_endpoint_received(uint8_t endpoint, void *buf, uint16_t len)
{
    if (!usb_app_setup)
        return;

    FOREACH_INTERFACE(node)
    {
        if (!node->interface->hook_usb_endpoint_received)
            continue;

        (*node->interface->hook_usb_endpoint_received)(endpoint, buf, len);
    }
}

void hook_usb_endpoint_sent(uint8_t endpoint, void *buf, uint16_t len)
{
    if (!usb_app_setup)
        return;

    FOREACH_INTERFACE(node)
    {
        if (!node->interface->hook_usb_endpoint_sent)
            continue;

        (*node->interface->hook_usb_endpoint_sent)(endpoint, buf, len);
    }
}

