/**
 * USB Midi-Fader
 *
 * Configuration interface
 *
 * Kevin Cuzner
 */

#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

#include "storage.h"
#include "_gen_storage.h"

#define CFG_MODE_CTL  0
#define CFG_MODE_NOTE 1
#define CFG_MODE_PITCH 2

#define CONFIGURATION_HID_GET_PARAM 0x40
#define CONFIGURATION_HID_SET_PARAM 0x80

#endif //_CONFIGURATION_H_

