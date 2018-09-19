/**
 * USB Midi-Fader
 *
 * Buttons and LEDs
 *
 * Kevin Cuzner
 */

#ifndef _BUTTONS_H_
#define _BUTTONS_H_

#include <stdint.h>
#include <stddef.h>

/**
 * Initializes the buttons and LED system
 */
void buttons_init(void);

/**
 * Reads the count of the buttons attached to the device.
 *
 * This also corresponds to the number of fader channels and the number of
 * LEDs.
 */
uint8_t buttons_get_count(void);

/**
 * Reads the latest button mask
 *
 * LSB is button zero, aligned with fader zero
 */
uint8_t buttons_read(void);

/**
 * Writes the LED mask
 *
 * LSB is button/LED zero, aligned with fader zero
 */
void buttons_write_leds(uint8_t leds);

#endif //_BUTTONS_H_

