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
#include <stdbool.h>

#define STORAGE_INVALID_PARAMETER 0x0000
#define STORAGE_ERASED_PARAMETER 0xFFFF
#define STORAGE_INVALID_SIZE 0xFFFF

/**
 * Structure used by the storage generator to store data
 */
typedef struct StoredValue {
    uint16_t parameter;
    uint16_t size;
    uint8_t data[];
} __attribute__((packed)) StoredValue;

/**
 * Reads a parameter from the storage area.
 *
 * parameter: Parameter identifier
 * buf: Buffer in which to place the data
 * len: Length of the buffer in bytes, replaced with the actual read
 * length
 *
 * Returns if a complete stored value was read
 */
bool storage_read(uint16_t parameter, void *buf, size_t *len);

/**
 * Writes a parameter into the storage area.
 *
 * parameter: Parameter identifier
 * buf: Buffer from which to read the data
 * len: Length of the data
 *
 * Returns whether or not the write was successful
 */
bool storage_write(uint16_t parameter, void *buf, size_t len);

#endif //_STORAGE_H_

