/**
 * USB Midi-Fader
 *
 * Configuration interface
 *
 * Kevin Cuzner
 */

#include "configuration.h"

#include <string.h>

#include "stm32f0xx.h"
#include "usb_hid.h"

typedef struct {
    uint32_t command;
    union {
        uint32_t parameters[15];
        uint8_t buffer[60];
    };
} __attribute__((packed)) HIDBuffer;

/**
 * HID report buffer for the incoming configuration command
 */
static HIDBuffer configuration_command;

/**
 * HID report buffer for the outgoing configuration response
 */
static HIDBuffer configuration_response;

/**
 * Begins the reception of a request
 */
static void configuration_begin_request(void)
{
    USBTransferData transfer = {
        .addr = &configuration_command,
        .len = sizeof(configuration_command),
    };
    usb_hid_receive(&transfer);
}

/**
 * Gets the response to the get parameter command
 */
static void configuration_get_param_response(const HIDBuffer *request,
        HIDBuffer *response)
{
    uint32_t value = 0;

    // get the parameter
    size_t len = sizeof(value);
    __disable_irq();
    response->parameters[0] = !storage_read(request->parameters[1], &value, &len);
    __enable_irq();

    response->parameters[1] = request->parameters[1];
    response->parameters[2] = value;
    response->parameters[3] = len;
}

static void configuration_set_param_response(const HIDBuffer *request,
        HIDBuffer *response)
{

    //sanitize the length
    size_t len = request->parameters[3];
    if (len > sizeof(request->parameters[2]))
        len = sizeof(request->parameters[2]);

    __disable_irq();
    response->parameters[0] = !storage_write(request->parameters[1],
            &request->parameters[2], len);
    __enable_irq();
}

/**
 * Handles the reception of a request
 */
static void configuration_end_request(HIDBuffer *request)
{

    // We always echo the command
    configuration_response.command = request->command;

    //reset the response parameters
    memset(configuration_response.parameters, 0, sizeof(configuration_response.parameters)); 

    switch (request->command)
    {
        case CONFIGURATION_HID_GET_PARAM:
            configuration_get_param_response(request, &configuration_response);
            break;
        case CONFIGURATION_HID_SET_PARAM:
            configuration_set_param_response(request, &configuration_response);
            break;
        default:
            configuration_response.parameters[0] = 2;
            break;
    }

    USBTransferData transfer = {
        .addr = &configuration_response,
        .len = sizeof(configuration_response),
    };
    usb_hid_send(&transfer);
}

void hook_usb_hid_configured(void)
{
    configuration_begin_request();
}

void hook_usb_hid_out_report_received(const USBTransferData *report)
{
    if (report->addr != &configuration_command)
        return;

    configuration_end_request(&configuration_command);
}

void hook_usb_hid_in_report_sent(const USBTransferData *report)
{
    if (report->addr != &configuration_response)
        return;

    configuration_begin_request();
}

