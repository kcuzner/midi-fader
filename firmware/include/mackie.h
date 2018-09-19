/**
 * USB Midi-Fader
 *
 * Mackie control emulation
 *
 * Kevin Cuzner
 */

#ifndef _MACKIE_H_
#define _MACKIE_H_

#include <stdint.h>
#include <stddef.h>

/**
 * Initializes the mackie enumulator
 */
void mackie_init(void);

/**
 * Tick function which should be called periodically in the main thread of
 * execution
 */
void mackie_tick(void);

#endif //_MACKIE_H_

