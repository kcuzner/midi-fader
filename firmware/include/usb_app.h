/**
 * STM32 USB Peripheral Driver
 *
 * USB Application Layer
 *
 * Kevin Cuzner
 */
#ifndef _USB_APP_H_
#define _USB_APP_H_


#include <stdint.h>

#include "usb.h"

/**
 * The USB Application Layer provides an interface between the core USB
 * abstraction layer and a set of USB interfaces. This allows a single
 * device to smoothly enumerate multiple interfaces. While some
 * efficiency is lost by using this layer, the software development
 * burden should be lessened considerably for building devices without
 * needing to modify the core USB abstraction layer.
 */

/**
 * Somewhat more verbose USB Control Result which encodes an "unhandled"
 * status, allowing fallthrough handlers.
 */
typedef enum {
    USB_APP_CTL_OK = USB_CTL_OK,
    USB_APP_CTL_STALL = USB_CTL_STALL,
    USB_APP_CTL_UNHANDLED
} USBAppControlResult;

/**
 * Common hook type for hooks which have no parameters
 */
typedef void (*USBNoParameterHook)(void);

/**
 * Hook type for when a non-standard setup request arrives on endpoint
 * zero.
 *
 * Returns whether to continue with the control request, stall the
 * endpoint, or fallthrough to the next handler. If all handlers fall
 * through, the application will return a stall.
 */
typedef USBAppControlResult (*USBHandleControlSetupHook)(USBSetupPacket const *setup, USBTransferData *nextTransfer);

/**
 * Hook type for when the status stage of a setup request is completed
 * on endpoint zero.
 */
typedef void (*USBHandleControlCompleteHook)(USBSetupPacket const *setup);

/**
 * Hook type for when a SET_CONFIGURATION is received.
 */
typedef void (*USBSetConfigurationHook)(uint16_t configuration);

/**
 * Hook type for when a SET_INTERFACE is received.
 */
typedef void (*USBSetInterfaceHook)(uint16_t interface);

/**
 * Hook type for when data has been received into the latest buffer set
 * up by usb_endpoint_receive
 */
typedef void (*USBEndpointReceivedHook)(uint8_t endpoint, void *buf, uint16_t len);

/**
 * Hook type for when been fully sent from the latest buffer set up by
 * usb_endpoint_send
 */
typedef void (*USBEndpointSentHook)(uint8_t endpoint, void *buf, uint16_t len);

/**
 * Structure instantiated by each interface
 *
 * This is intended to usually be a static constant, but it could also
 * be created on the fly.
 */
typedef struct {
    /**
     * Hook function called when a USB reset occurs
     */
    USBNoParameterHook hook_usb_reset;
    /**
     * Hook function called when a setup request is received
     */
    USBHandleControlSetupHook hook_usb_handle_setup_request;
    /**
     * Hook function called when the status stage of a setup request is
     * completed on endpoint zero.
     */
    USBHandleControlCompleteHook hook_usb_control_complete;
    /**
     * Hook function called when a SOF is received
     */
    USBNoParameterHook hook_usb_sof;
    /**
     * Hook function called when a SET_CONFIGURATION is received
     */
    USBSetConfigurationHook hook_usb_set_configuration;
    /**
     * Hook function called when a SET_INTERFACE is received
     */
    USBSetInterfaceHook hook_usb_set_interface;
    /**
     * Hook function called when data is received on a USB endpoint
     */
    USBEndpointReceivedHook hook_usb_endpoint_received;
    /**
     * Hook function called when data is sent on a USB endpoint
     */
    USBEndpointSentHook hook_usb_endpoint_sent;
} USBInterface;

/**
 * Node structure for interfaces attached to the USB device
 */
typedef struct USBInterfaceListNode {
    const USBInterface *interface;
    const struct USBInterfaceListNode *next;
} USBInterfaceListNode;

typedef struct {
    /**
     * Hook function called when the USB peripheral is reset
     */
    USBNoParameterHook hook_usb_reset;
    /**
     * Hook function called when a SOF is received.
     */
    USBNoParameterHook hook_usb_sof;
    /**
     * Head of the interface list. This node will be visited first
     */
    const USBInterfaceListNode *interface_list;
} USBApplicationSetup;

/**
 * USB setup constant
 *
 * Define this elsewhere, such as main
 */
extern const USBApplicationSetup *usb_app_setup;

#endif //_USB_APP_H_

