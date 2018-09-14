/**
 * System tick abstraction
 *
 * USB Midi-Fader
 *
 * Kevin Cuzner
 */

#ifndef _SYSTICK_H_
#define _SYSTICK_H_

typedef void (*SystickHandler)(void);

/**
 * Initialize the systick
 */
void systick_init(void);

/**
 * Subscribe a function to be called during the systick ISR
 */
void systick_subscribe(SystickHandler handler);

#endif //_SYSTICK_H_

