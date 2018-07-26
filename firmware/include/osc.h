/**
 * USB Midi-Fader
 *
 * Kevin Cuzner
 *
 * Oscillator Control
 */

#ifndef _OSC_H_
#define _OSC_H_

#include <stdint.h>

#define OSC_MAX_CALLBACKS 16

typedef void (*OscChangeCallback)(void);

/**
 * Requests the primary oscillator to change to HSI8
 */
void osc_request_hsi8(void);

/**
 * Requests the primary oscillator to change to HSI8 through the PLL with some
 * multiplication factor.
 *
 * prediv: The oscillator will be divided by this value plus one. Maximum is 15.
 * mul: The oscillator will be multiplied by this value plus two. Maximum is 14.
 */
void osc_request_hsi8_pll(uint8_t prediv, uint8_t mul);

/**
 * Starts the HSI48 oscillator without switching the system clock over.
 *
 * This is used for functionality that depends on the HSI48 so that we can keep
 * oscillator selection confined to this module.
 */
void osc_start_hsi48(void);

/**
 * Requests the primary oscillator to change to HSI48
 */
void osc_request_hsi48(void);

/**
 * Adds a callback function to the list called when the oscillator
 * frequency is changed
 */
void osc_add_callback(OscChangeCallback fn);

#endif //_OSC_H_

