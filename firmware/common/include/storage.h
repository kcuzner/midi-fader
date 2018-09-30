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

#include "error.h"

#define STORAGE_INVALID_PARAMETER 0x0000
#define STORAGE_ERASED_PARAMETER 0xFFFF
#define STORAGE_INVALID_SIZE 0xFFFF

// Warning when the buffer is too small for a read value
#define STORAGE_WRN_INSUFFICIENT_BUF 1000
// Error when there is no storage segment available
#define STORAGE_ERR_NO_STORAGE -1001
// Error when migration fails due to the magic value being already
// programmed
#define STORAGE_ERR_MIGRATE_MAGIC -1002
// Error when the value cannot be found
#define STORAGE_ERR_NOT_FOUND -1003
// Error when the storage region contains invalid data
#define STORAGE_ERR_CORRUPT -1004
// Error when a write fails because the value is too large, even after
// migration
#define STORAGE_ERR_TOO_LARGE -1005

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
 * This must be called with interrupts disabled!
 *
 * parameter: Parameter identifier
 * buf: Buffer in which to place the data
 * len: Length of the buffer in bytes, replaced with the actual read
 * length
 */
void storage_read(uint16_t parameter, void *buf, size_t *len, Error err);

/**
 * Writes a parameter into the storage area.
 *
 * This must be called with interrupts disabled!
 *
 * parameter: Parameter identifier
 * buf: Buffer from which to read the data
 * len: Length of the data
 *
 * Returns whether or not the write was successful
 */
void storage_write(uint16_t parameter, const void *buf, size_t len, Error err);

#endif //_STORAGE_H_

