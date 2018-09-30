/**
 * USB Midi-Fader
 *
 * Storage system using the flash
 *
 * Kevin Cuzner
 */

#include "storage.h"
#include "_gen_storage.h"

#include "stm32f0xx.h"
#include "nvm.h"

#include <string.h>
#include <stdbool.h>

/**
 * The motivation for this system is the fact that we don't have an EEPROM.
 * This storage system will attempt to level the wear across the storage
 * segment of memory as data is written. It operates as a linked list which
 * contains a 32-bit identifier and a size. Immediately after the size member
 * is the data buffer that contains the actual data for the value. The next
 * data value appears *size* bytes after the size member.
 *
 * The storage segment is divided into two portions:
 * - Segment A starts at _astorage and ends the byte before _bstorage_magic.
 * - Segment B starts at _bstorage and ends the byte before _estorage.
 *
 * The effective storage space is limited to half of the storage segment size.
 *
 * Each storage segment is marked as active with a magic word that appears at
 * the start of each segment. When deciding which segment to walk, the first
 * segment (starting with A) which has a valid magic word is the one that is
 * walked.
 *
 * When values are written, the original parameter is located and its parameter
 * field zeroed out (Writing 0x0000 is valid to non-erased locations). The list
 * is then walked until a parameter reads 0xFFFF or the end of the segment is
 * reached.
 *
 * In the case where the end of a segment is reached, this indicates that the
 * segment is filled and needs to be erased. The segment is walked from the
 * start and all valid parameters are re-written to the opposite segment. The
 * now-unused segment is erased in its entirety.
 */

/**
 * This contains constants that indicate the state of the storage "a"
 * segment. When it is 0xFFFF the storage has been erased and can be
 * used for a migration. When it is STORAGE_SECTION_START_MAGIC, this
 * section is currently in use.
 */
extern uint16_t _storagea_magic;

/**
 * This symbol is defined by the linker script at the beginning of the
 * storage section. We pretend that it contains a StoredValue struct.
 */
extern StoredValue _sstoragea;

/**
 * Same purpose as the storagea_magic variable, but for the B segment.
 */
extern uint16_t _storageb_magic;

/**
 * This symbols id defined by the linker script in the middle of the
 * storage section.
 */
extern StoredValue _sstorageb;

/**
 * This symbol is defined by the linker script at the end of the storage
 * section. We pretend it contains a stored value struct, while in
 * reality most of bits bits lie outside flash.
 */
extern StoredValue _estorage;

/**
 * Locates the start and end of the current segment
 *
 * If a valid storage segment cannot be found, both values are set to
 * zero or null.
 */
static void storage_get_start_end(StoredValue **start, uintptr_t *end, Error err)
{
    if (ERROR_IS_FATAL(err))
        return;

    if (_storagea_magic == STORAGE_SECTION_START_MAGIC)
    {
        *start = &_sstoragea;
        *end = (uintptr_t)(&_storageb_magic) - 1;
    }
    else if (_storageb_magic == STORAGE_SECTION_START_MAGIC)
    {
        *start = &_sstorageb;
        *end = (uintptr_t)&_estorage;
    }
    else
    {
        ERROR_SET(err, STORAGE_ERR_NO_STORAGE);
    }
}

/**
 * Computes the location of the next stored address
 *
 * addr: pointer-integer to the stored value
 * size: Value of the size parameter in the StoredValue
 */
static uintptr_t storage_get_next_stored_address(uintptr_t addr, uint16_t size)
{
    addr += sizeof(StoredValue) + size;
    if (addr % 4 != 0)
        addr += 4 - addr % 4;
    return addr;
}

/**
 * Returns a pointer to the next stored value after the passed value.
 */
static StoredValue* storage_get_next_stored(StoredValue *current)
{
    return (StoredValue *)storage_get_next_stored_address((uintptr_t)current, current->size);
}

/**
 * Walks storage and locates the parameter
 *
 * parameter: Parameter to find
 *
 * Returns NULL if the parameter was not found, otherwise it returns a
 * pointer to the const data.
 *
 * This does not set STORAGE_ERR_NOT_FOUND
 */
static StoredValue *storage_find(uint16_t parameter, Error err)
{
    StoredValue *current;
    uintptr_t end;

    if (ERROR_IS_FATAL(err))
        return NULL;

    storage_get_start_end(&current, &end, err);

    // If we don't have valid storage segment, we're done
    if (ERROR_IS_FATAL(err))
        return NULL;

    // Walk the storage, locating the parameter
    //
    // If the parameter is erased, then we cannot trust any bytes after
    // it and we are done walking.
    while ((uintptr_t)current < end && current->parameter != parameter &&
            current->parameter != STORAGE_ERASED_PARAMETER)
    {
        //move the pointer
        current = storage_get_next_stored(current);
    }

    if ((uintptr_t)current >= end || current->parameter == STORAGE_ERASED_PARAMETER)
    {
        return NULL;
    }
    else
    {
        return current;
    }
}

void storage_read(uint16_t parameter, void *buf, size_t *len, Error err)
{
    if (ERROR_IS_FATAL(err))
        return;

    StoredValue *value = storage_find(parameter, err);
    if (ERROR_IS_FATAL(err))
        return;
    if (!value)
    {
        ERROR_SET(err, STORAGE_ERR_NOT_FOUND);
    }

    size_t readLen = value->size;
    if (*len < readLen)
    {
        readLen = *len;
        ERROR_SET(err, STORAGE_WRN_INSUFFICIENT_BUF);
    }
    memcpy(buf, value->data, readLen);
    *len = readLen;
}

static void storage_flash_write_stored_value(StoredValue *location, uint16_t parameter,
        uint16_t len, const void *buf, Error err)
{
    if (ERROR_IS_FATAL(err))
        return;

    uint8_t *bytes = (uint8_t *)buf;
    for (size_t idx = 0; idx < len; idx += 2)
    {
        uint16_t halfword = bytes[idx];
        if (idx + 1 < len)
            halfword |= ((uint16_t)bytes[idx+1]) << 8;
        // I promise that idx is an even number and is uint16_t*
        // aligned.
        nvm_flash_write((uint16_t*)(&location->data[idx]), halfword, err);
        if (ERROR_IS_FATAL(err))
            return;
    }
    // Size is written next.
    nvm_flash_write(&location->size, len, err);
    if (ERROR_IS_FATAL(err))
        return;
    // Parameter is last. The entry is now considered valid and will
    // be walked.
    nvm_flash_write(&location->parameter, parameter, err);
}

/**
 * Copies and consolidates the current storage segment into the other
 * one
 */
static void storage_migrate(Error err)
{
    if (ERROR_IS_FATAL(err))
        return;
    StoredValue *src;
    uintptr_t end;
    storage_get_start_end(&src, &end, err);

    //If there isn't a current section, we can't migrate anything
    if (ERROR_IS_FATAL(err))
        return;

    //Determine the destination page
    StoredValue *dest = src == &_sstoragea ? &_sstorageb : &_sstoragea;
    uint16_t *magicsrc = src == &_sstoragea ? &_storagea_magic : &_storageb_magic;
    uint16_t *magicdest = src == &_sstoragea ? &_storageb_magic : &_storagea_magic;

    //validate that the destination is not active
    //TODO: Do we want to actually check if its erased instead?
    if (*magicdest == STORAGE_SECTION_START_MAGIC)
    {
        ERROR_SET(err, STORAGE_ERR_MIGRATE_MAGIC);
        return;
    }

    // Iterate the source and write all valid parameters to the
    // destination
    while ((uintptr_t)src < end && src->parameter != STORAGE_ERASED_PARAMETER &&
            src->size != STORAGE_INVALID_SIZE)
    {
        if (src->parameter != STORAGE_INVALID_PARAMETER)
        {
            //write this to destination
            storage_flash_write_stored_value(dest, src->parameter, src->size, src->data, err);
            if (ERROR_IS_FATAL(err))
                return;
            dest = storage_get_next_stored(dest);
        }
        src = storage_get_next_stored(src);
    }

    // This section is now fully migrated and is good
    nvm_flash_write(magicdest, STORAGE_SECTION_START_MAGIC, err);
    if (ERROR_IS_FATAL(err))
        return;

    // any pointer within the page will erase the entire page
    nvm_flash_erase_page(magicsrc, err);
}

static StoredValue *storage_find_end(Error err)
{
    StoredValue *start;
    uintptr_t end;
    storage_get_start_end(&start, &end, err);

    StoredValue *next = start;
    while ((uintptr_t)next < end && next->parameter != STORAGE_ERASED_PARAMETER)
    {
        next = storage_get_next_stored(next);
    }

    if ((uintptr_t)next > end)
    {
        // There is some corrupt data forcing us beyond the end
        ERROR_SET(err, STORAGE_ERR_CORRUPT);
        return NULL;
    }
    return next;
}

uintptr_t last_start;

void storage_write(uint16_t parameter, const void *buf, size_t len, Error err)
{
    if (ERROR_IS_FATAL(err))
        return;

    StoredValue *start;
    uintptr_t end;

    // Important note: There are several cases here that result in a
    // corrupted storage segment. I'm really not sure what to do about
    // these. I also don't handle things like surprise power removal
    // very well. Basically, a write could be very fragile.

    storage_get_start_end(&start, &end, err);
    if (ERROR_IS_FATAL(err))
        return;

    StoredValue *current = storage_find(parameter, err);
    if (ERROR_IS_FATAL(err))
        return;

    // Walk the storage until we come to the next free space
    // or we reach the end
    StoredValue *next = storage_find_end(err);
    if (ERROR_IS_FATAL(err))
        return;

    last_start = (uintptr_t)next;

    // Compute the end address of the block
    uintptr_t endaddr = storage_get_next_stored_address((uintptr_t)next, len);

    // Perform a migration if necessary
    if ((uintptr_t)next > end || endaddr > end)
    {
        //we have reached the end of the segment. Migrate to the other
        //segment.
        storage_migrate(err);
        if (ERROR_IS_FATAL(err))
            return;

        //recompute the segment boundaries
        storage_get_start_end(&start, &end, err);
        if (ERROR_IS_FATAL(err))
            return;

        //re-locate the original value
        current = storage_find(parameter, err);
        if (ERROR_IS_FATAL(err))
            return;
        if (current == NULL)
        {
            //This is a big problem, we've now lost a value.
            ERROR_SET(err, STORAGE_ERR_CORRUPT);
            return;
        }

        //locate the next free spot
        next = storage_find_end(err);
        if (ERROR_IS_FATAL(err))
            return;

        endaddr = storage_get_next_stored_address((uintptr_t)next, len);
        if ((uintptr_t)next > end || endaddr > end)
        {
            //there is no space large enough to hold this after
            //consolidation
            ERROR_SET(err, STORAGE_ERR_TOO_LARGE);
        }
    }

    // Write the new value
    storage_flash_write_stored_value(next, parameter, len, buf, err);

    // Invalidate the old value
    nvm_flash_write(&(current->parameter), STORAGE_INVALID_PARAMETER, err);
}

