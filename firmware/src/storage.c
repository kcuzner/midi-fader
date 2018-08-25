/**
 * USB Midi-Fader
 *
 * Storage system using the flash
 *
 * Kevin Cuzner
 */

#include "storage.h"

#include <string.h>

/**
 * The motivation for this system is the fact that we don't have an
 * EEPROM. This storage system will attempt to level the wear across the
 * storage segment of memory as data is written. It operates as a linked
 * list which contains a 32-bit identifier and a pointer to the next
 * stored value. Immediately after the pointer is the data buffer that
 * contains the actual data for the value.
 */

/**
 * This symbol is defined by the linker script at the beginning of the
 * storage section. We pretend that it contains a StoredValue struct.
 */
extern StoredValue _sstorage;

static StoredValue *storage_find(uint16_t parameter)
{
    StoredValue *current = &_sstorage;
    while (current->parameter != parameter && current->size != INVALID_SIZE)
    {
        //move the pointer
        uintptr_t addr = (uintptr_t)current;
        addr += sizeof(StoredValue) + current->size;
        if (addr % 4 != 0)
            addr += 4 - addr % 4;
        current = (StoredValue *)addr;
    }

    if (current->size == INVALID_SIZE)
    {
        return NULL;
    }
    else
    {
        return current;
    }
}

size_t storage_read(uint16_t parameter, void *buf, size_t len)
{
    StoredValue *value = storage_find(parameter);
    if (value == NULL)
    {
        return 0;
    }
    else
    {
        size_t readLen = value->size;
        if (len > readLen)
            readLen = len;
        memcpy(buf, value->data, readLen);
        return readLen;
    }
}

