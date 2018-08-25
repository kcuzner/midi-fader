/**
 * USB Midi-Fader
 *
 * Storage system using the flash
 *
 * Kevin Cuzner
 */

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <stddef.h>
#include <stdint.h>

#define INVALID_PARAMETER 0x00000000
#define INVALID_SIZE 0xFFFFFFFF

/**
 * Structure used by the storage generator to store data
 */
typedef struct StoredValue {
    uint16_t parameter;
    uint16_t size;
    uint8_t data[];
} StoredValue;

/**
 * Reads a parameter from the storage area
 *
 * parameter: Parameter identifier
 * buf: Buffer in which to place the data
 * len: Length of the buffer in bytes
 *
 * Returns actual size of the read parameter
 */
size_t storage_read(uint16_t parameter, void *buf, size_t len);

/**
 * Writes a parameter into the storage area
 *
 * paramter: Parameter identifier
 * buf: Buffer from which to read the data
 * len: Length of the data
 */
void storage_write(uint16_t parameter, void *buf, size_t *len);

#endif //_STORAGE_H_

