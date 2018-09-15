/**
 * USB Midi-Fader
 *
 * Configuration interface
 *
 * Kevin Cuzner
 */

#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

#include "error.h"

#define CFG_MODE_CTL  0
#define CFG_MODE_NOTE 1
#define CFG_MODE_PITCH 2

#define CONFIGURATION_HID_GET_PARAM 0x40
#define CONFIGURATION_HID_SET_PARAM 0x80

/**
 * Gets the number of 1ms ticks to delay for sending control changes
 */
uint32_t configuration_event_tick_delay(Error err);

/**
 * Gets the configured channel for a button
 */
uint8_t configuration_btn_channel(uint8_t button, Error err);

/**
 * Gets the configured button mode
 */
uint8_t configuration_btn_mode(uint8_t button, Error err);

/**
 * Gets the configured button controller number
 */
uint8_t configuration_btn_cc(uint8_t button, Error err);

/**
 * Gets the configured "ON" control value for a button
 */
uint8_t configuration_btn_cc_on(uint8_t button, Error err);

/**
 * Gets the configured "OFF" control value for a button
 */
uint8_t configuration_btn_cc_off(uint8_t button, Error err);

/**
 * Gets the configured note for a button
 */
uint8_t configuration_btn_note(uint8_t button, Error err);

/**
 * Gets the configured channel for a fader
 */
uint8_t configuration_fdr_channel(uint8_t fader, Error err);

/**
 * Gets the fader mode (cc or pitch)
 */
uint8_t configuration_fdr_mode(uint8_t fader, Error err);

/**
 * Gets the controller number for a fader
 */
uint8_t configuration_fdr_cc(uint8_t fader, Error err);

/**
 * Gets the minimum fader controller value
 */
uint8_t configuration_fdr_cc_min(uint8_t fader, Error err);

/**
 * Gets the maximum fader controller value
 */
uint8_t configuration_fdr_cc_max(uint8_t fader, Error err);

/**
 * Gets the minimum fader pitch value
 */
int16_t configuration_fdr_pitch_min(uint8_t fader, Error err);

/**
 * Gets the maximum fader pitch value
 */
int16_t configuration_fdr_pitch_max(uint8_t fader, Error err);

#endif //_CONFIGURATION_H_

