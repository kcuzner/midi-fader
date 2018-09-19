/**
 * USB Midi-Fader
 *
 * Configuration interface
 *
 * Kevin Cuzner
 */

/**
 * Theory of operation
 *
 * This is fairly straightforward. This uses the storage system to keep
 * parameters which are used for configuring the various attributes of
 * the device. It provides getter methods to the application which can
 * be used to get the configuration values on a per-button or per-fader
 * basis. This basically just maps those calls to the appropriate
 * storage calls so that the user application can be agnostic of the
 * _gen_storage constants.
 *
 * In addition, this implements the application side of the HID
 * interface. This interface is used for configuration of the device
 * from host software.
 */

#include "configuration.h"

#include <string.h>

#include "stm32f0xx.h"
#include "storage.h"
#include "_gen_storage.h"
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

#define CFG_PARAM(TYPE, IDX, PARAM) STORAGE_ ## TYPE ## IDX ## _ ## PARAM

/**
 * Struct for organizing button parameter constants
 */
typedef struct {
    uint16_t channel;
    uint16_t mode;
    uint16_t cc;
    uint16_t cc_on;
    uint16_t cc_off;
    uint16_t note;
    uint16_t note_vel;
    uint16_t style;
} CfgButtonParameters;

#define CFG_BTN_PARAMS(IDX) {\
    .channel = CFG_PARAM(BTN, IDX, CH),\
    .mode = CFG_PARAM(BTN, IDX, MODE),\
    .cc = CFG_PARAM(BTN, IDX, CC),\
    .cc_on = CFG_PARAM(BTN, IDX, CC_ON),\
    .cc_off = CFG_PARAM(BTN, IDX, CC_OFF),\
    .note = CFG_PARAM(BTN, IDX, NOTE),\
    .note_vel = CFG_PARAM(BTN, IDX, NOTE_VEL),\
    .style = CFG_PARAM(BTN, IDX, STYLE),\
}

/**
 * Struct for organizing fader parameter constants
 */
typedef struct {
    uint16_t channel;
    uint16_t mode;
    uint16_t cc;
    uint16_t cc_min;
    uint16_t cc_max;
    uint16_t pitch_min;
    uint16_t pitch_max;
} CfgFaderParameters;

#define CFG_FDR_PARAMS(IDX) {\
    .channel = CFG_PARAM(FDR, IDX, CH),\
    .mode = CFG_PARAM(FDR, IDX, MODE),\
    .cc = CFG_PARAM(FDR, IDX, CC),\
    .cc_min = CFG_PARAM(FDR, IDX, CC_MIN),\
    .cc_max = CFG_PARAM(FDR, IDX, CC_MAX),\
    .pitch_min = CFG_PARAM(FDR, IDX, PITCH_MIN),\
    .pitch_max = CFG_PARAM(FDR, IDX, PITCH_MAX),\
}

/**
 * Button configuration parameters per-button
 */
static const CfgButtonParameters cfg_button_params[8] = {
    CFG_BTN_PARAMS(0),
    CFG_BTN_PARAMS(1),
    CFG_BTN_PARAMS(2),
    CFG_BTN_PARAMS(3),
    CFG_BTN_PARAMS(4),
    CFG_BTN_PARAMS(5),
    CFG_BTN_PARAMS(6),
    CFG_BTN_PARAMS(7),
};

/**
 * Fader configuration parameters per-fader
 */
static const CfgFaderParameters cfg_fader_params[8] = {
    CFG_FDR_PARAMS(0),
    CFG_FDR_PARAMS(1),
    CFG_FDR_PARAMS(2),
    CFG_FDR_PARAMS(3),
    CFG_FDR_PARAMS(4),
    CFG_FDR_PARAMS(5),
    CFG_FDR_PARAMS(6),
    CFG_FDR_PARAMS(7),
};

#define CFG_READ(TYPE, PARAMS, IDX, PARAM) {\
    TYPE param = 0;\
    size_t len = sizeof(param);\
    storage_read(PARAMS[IDX].PARAM, &param, &len, err);\
    return param;\
}

uint32_t configuration_event_tick_delay(Error err)
{
    uint32_t param = 0;
    size_t len = sizeof(param);
    storage_read(STORAGE_EVENT_TICK_DELAY, &param, &len, err);
    return param;
}

uint8_t configuration_btn_channel(uint8_t button, Error err)
{
    CFG_READ(uint8_t, cfg_button_params, button, channel);
}

uint8_t configuration_btn_mode(uint8_t button, Error err)
{
    CFG_READ(uint8_t, cfg_button_params, button, mode);
}

uint8_t configuration_btn_cc(uint8_t button, Error err)
{
    CFG_READ(uint8_t, cfg_button_params, button, cc);
}

uint8_t configuration_btn_cc_on(uint8_t button, Error err)
{
    CFG_READ(uint8_t, cfg_button_params, button, cc_on);
}

uint8_t configuration_btn_cc_off(uint8_t button, Error err)
{
    CFG_READ(uint8_t, cfg_button_params, button, cc_off);
}

uint8_t configuration_btn_note(uint8_t button, Error err)
{
    CFG_READ(uint8_t, cfg_button_params, button, note);
}

uint8_t configuration_btn_note_vel(uint8_t button, Error err)
{
    CFG_READ(uint8_t, cfg_button_params, button, note_vel);
}

uint8_t configuration_btn_style(uint8_t button, Error err)
{
    CFG_READ(uint8_t, cfg_button_params, button, style);
}

uint8_t configuration_fdr_channel(uint8_t fader, Error err)
{
    CFG_READ(uint8_t, cfg_fader_params, fader, channel);
}

uint8_t configuration_fdr_mode(uint8_t fader, Error err)
{
    CFG_READ(uint8_t, cfg_fader_params, fader, mode);
}

uint8_t configuration_fdr_cc(uint8_t fader, Error err)
{
    CFG_READ(uint8_t, cfg_fader_params, fader, cc);
}

uint8_t configuration_fdr_cc_min(uint8_t fader, Error err)
{
    CFG_READ(uint8_t, cfg_fader_params, fader, cc_min);
}

uint8_t configuration_fdr_cc_max(uint8_t fader, Error err)
{
    CFG_READ(uint8_t, cfg_fader_params, fader, cc_max);
}

int16_t configuration_fdr_pitch_min(uint8_t fader, Error err)
{
    CFG_READ(int16_t, cfg_fader_params, fader, pitch_min);
}

int16_t configuration_fdr_pitch_max(uint8_t fader, Error err)
{
    CFG_READ(int16_t, cfg_fader_params, fader, pitch_max);
}


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
    ERROR_INST(err);
    uint32_t value = 0;

    // get the parameter
    size_t len = sizeof(value);
    __disable_irq();
    storage_read(request->parameters[1], &value, &len, ERROR_FROM_INST(err));
    response->parameters[0] = err;
    __enable_irq();

    response->parameters[1] = request->parameters[1];
    response->parameters[2] = value;
    response->parameters[3] = len;
}

static void configuration_set_param_response(const HIDBuffer *request,
        HIDBuffer *response)
{
    ERROR_INST(err);

    //sanitize the length
    size_t len = request->parameters[3];
    if (len > sizeof(request->parameters[2]))
        len = sizeof(request->parameters[2]);

    __disable_irq();
    storage_write(request->parameters[1], &request->parameters[2], len, ERROR_FROM_INST(err));
    response->parameters[0] = err;
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

