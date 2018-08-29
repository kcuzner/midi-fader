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

#include <string.h>
#include <stdbool.h>

/**
 * The motivation for this system is the fact that we don't have an
 * EEPROM. This storage system will attempt to level the wear across the
 * storage segment of memory as data is written. It operates as a linked
 * list which contains a 32-bit identifier and a pointer to the next
 * stored value. Immediately after the pointer is the data buffer that
 * contains the actual data for the value.
 *
 * The storage segment is divided into two portions:
 * - Segment 1 starts at _sstorage and ends the byte before _mstorage.
 * - Segment 2 starts at _mstorage and ends the byte before _estorage.
 *
 * The effective storage space is limited to half of the storage size.
 *
 * Reading proceeds by first deciding which segment to start walking.
 * Walking always starts at _sstorage. If the parameter at that location
 * shows that it is erased, the walking then jumps to _mstorage.
 *
 * When values are written, the original parameter is located and its
 * parameter field zeroed out (Writing 0x0000 is valid to non-erased
 * locations). The list is then walked until a parameter reads 0xFFFF or
 * the end of the segment is reached.
 *
 * In the case where the end of a segment is reached, this indicates
 * that the segment is filled and needs to be erased. The segment is
 * walked from the start and all valid parameters are re-written to the
 * opposite segment. The now-unused segment is erased in its entirety.
 */

/**
 * Certain functions, such as flash write, are easier to do if the code is
 * executed from the RAM. This decoration relocates the function there and
 * prevents any inlining that might otherwise move the function to flash.
 */
#define _RAM __attribute__((section (".data#"), noinline))

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
static void storage_get_start_end(StoredValue **start, uintptr_t *end)
{
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
        *start = NULL;
        *end = 0;
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
 * pointer to the const data. If the passed parameter is
 * STORAGE_INVALID_PARAMETER, this only returns null if we've gone
 * beyond the boundaries of the segment.
 */
static StoredValue *storage_find(uint16_t parameter)
{
    StoredValue *current;
    uintptr_t end;
    storage_get_start_end(&current, &end);

    // If we don't have valid storage segment, we're done
    if (!current || !end)
        return NULL;

    // Walk the storage, locating the parameter
    //
    // If the parameter is erased, then we cannot trust any bytes after
    // it and we are done walking.
    while ((uintptr_t)current < end &&
            (parameter != STORAGE_INVALID_PARAMETER && current->parameter != parameter) &&
            current->parameter != STORAGE_ERASED_PARAMETER)
    {
        //move the pointer
        current = storage_get_next_stored(current);
    }

    if ((uintptr_t)current >= end || (parameter != STORAGE_INVALID_PARAMETER &&
                current->parameter == STORAGE_ERASED_PARAMETER))
    {
        return NULL;
    }
    else
    {
        return current;
    }
}

bool storage_read(uint16_t parameter, void *buf, size_t *len)
{
    StoredValue *value = storage_find(parameter);
    if (value == NULL)
    {
        *len = 0;
        return false;
    }
    else
    {
        size_t readLen = value->size;
        if (*len > readLen)
            readLen = *len;
        memcpy(buf, value->data, readLen);
        if (readLen == *len)
            return true;
        *len = readLen;
        return false;
    }
}

/**
 * Unlocks the flash for write operations
 */
static void storage_unlock_flash(void)
{
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = 0x45670123;
        FLASH->KEYR = 0xCDEF89AB;
    }
}

/**
 * Locks the flash against write operations
 */
static void storage_lock(void)
{
    if (!(FLASH->CR & FLASH_CR_LOCK))
    {
        FLASH->CR |= FLASH_CR_LOCK;
    }
}

/**
 * RAM-located function which performs the half-word writes.
 *
 * address: Halfword-aligned write address
 * data: Halfword to write
 */
static _RAM bool storage_flash_do_write(uint16_t *addr, uint16_t data)
{
    //half-word program operation
    FLASH->CR |= FLASH_CR_PG;
    *addr = data;
    //wait for completion
    while (FLASH->SR & FLASH_SR_BSY) { }
    if (FLASH->SR & FLASH_SR_EOP)
    {
        FLASH->SR = FLASH_SR_EOP;
        return *addr == data;
    }
    else
    {
        FLASH->SR = FLASH_SR_WRPRTERR | FLASH_SR_PGERR;
        return false;
    }
}

/**
 * Wrapper function that prepares and performs a flash write
 *
 * addr: Halfword-aligned address to write
 * data: Halfword to write
 */
static bool storage_flash_write(uint16_t *addr, uint16_t data)
{
    bool result = false;
    storage_unlock_flash();
    result = storage_flash_do_write(addr, data);
    storage_lock();
    return result;
}

/**
 * RAM-located function which performs the page erase. Pages are 1KB in
 * size.
 *
 * pageaddr: Address located within the 1KB page to be erased.
 */
static _RAM bool storage_flash_do_erase_page(uint16_t *pageaddr)
{
    //page erase operation
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = (uint32_t)(pageaddr);
    FLASH->CR |= FLASH_CR_STRT;
    //wait for completion
    while (FLASH->SR & FLASH_SR_BSY) { }
    if (FLASH->SR & FLASH_SR_EOP)
    {
        FLASH->SR = FLASH_SR_EOP;
        return true;
    }
    else
    {
        FLASH->SR = FLASH_SR_WRPRTERR | FLASH_SR_PGERR;
        return false;
    }
}

/**
 * Wrapper function that prepares and performs a flash erase
 *
 * pageaddr: Address within the page to erase
 */
static bool storage_flash_erase_page(uint16_t *pageaddr)
{
    bool result = false;
    storage_unlock_flash();
    result = storage_flash_do_erase_page(pageaddr);
    storage_lock();
    return result;
}

static bool storage_flash_write_stored_value(StoredValue *location, uint16_t parameter,
        uint16_t len, const void *buf)
{
    uint8_t *bytes = (uint8_t *)buf;
    for (size_t idx = 0; idx < len; idx += 2)
    {
        uint16_t halfword = bytes[idx];
        if (idx + 1 < len)
            halfword |= ((uint16_t)bytes[idx+1]) << 8;
        // I promise that idx is an even number and is uint16_t*
        // aligned.
        if (!storage_flash_write((uint16_t*)(&location->data[idx]), halfword))
            return false;
    }
    // Size is written next.
    if (!storage_flash_write(&location->size, len))
        return false;
    // Parameter is last. The entry is now considered valid and will
    // be walked.
    return storage_flash_write(&location->parameter, parameter);
}

/**
 * Copies and consolidates the current storage segment into the other
 * one
 */
static bool storage_migrate()
{
    StoredValue *src;
    uintptr_t end;
    storage_get_start_end(&src, &end);

    //If there isn't a current section, we can't migrate anything
    if (!src || !end)
        return false;

    //Determine the destination page
    StoredValue *dest = src == &_sstoragea ? &_sstorageb : &_sstoragea;
    uint16_t *magicsrc = src == &_sstoragea ? &_storagea_magic : &_storageb_magic;
    uint16_t *magicdest = src == &_sstoragea ? &_storageb_magic : &_storagea_magic;

    //validate that the destination is not active
    //TODO: Do we want to actually check if its erased instead?
    if (*magicdest == STORAGE_SECTION_START_MAGIC)
        return false;

    // Iterate the source and write all valid parameters to the
    // destination
    while ((uintptr_t)src < end && src->parameter != STORAGE_ERASED_PARAMETER &&
            src->size != STORAGE_INVALID_SIZE)
    {
        if (src->parameter == STORAGE_INVALID_PARAMETER)
        {
            src = storage_get_next_stored(src);
        }
        else
        {
            //write this to destination
            if (!storage_flash_write_stored_value(dest, src->parameter, src->size,
                        src->data))
                return false;
            dest = storage_get_next_stored(dest);
        }
    }

    // This section is now fully migrated and is good
    if (!storage_flash_write(magicdest, STORAGE_SECTION_START_MAGIC))
        return false;

    // any pointer within the page will erase the entire page
    storage_flash_erase_page(magicsrc);
    return true;
}

bool storage_write(uint16_t parameter, const void *buf, size_t len)
{
    StoredValue *start;
    uintptr_t end;

    // Important note: There are several cases here that result in a
    // corrupted storage segment. I'm really not sure what to do about
    // these. I also don't handle things like surprise power removal
    // very well. Basically, a write could be very fragile.

    storage_get_start_end(&start, &end);
    if (!start || !end)
        return false;

    StoredValue *current = storage_find(parameter);
    if (current == NULL)
        return false;

    // Walk the storage until we come to the next free space
    // or we reach the end
    StoredValue *next = storage_find(STORAGE_INVALID_PARAMETER);
    if (!next)
        return false; //there is some corrupt data that has forced us beyond the end

    // Compute the end address of the block
    uintptr_t endaddr = storage_get_next_stored_address((uintptr_t)next, len);

    // Perform a migration if necessary
    if ((uintptr_t)next > end || endaddr > end)
    {
        //we have reached the end of the segment. Migrate to the other
        //segment.
        if (!storage_migrate())
            return false;

        //recompute the segment boundaries
        storage_get_start_end(&start, &end);

        //re-locate the original value
        current = storage_find(parameter);
        if (current == NULL)
            return false; //This is a big problem, we've now lost a value.

        //locate the next free spot
        next = storage_find(STORAGE_INVALID_PARAMETER);
        endaddr = storage_get_next_stored_address((uintptr_t)next, len);
        if ((uintptr_t)next > end || endaddr > end)
            return false; //there is no space large enough to hold this after consolidation
    }

    // Write the new value
    if (!storage_flash_write_stored_value(next, parameter, len, buf))
        return false;

    // Invalidate the old value
    return storage_flash_write(&(current->parameter), STORAGE_INVALID_PARAMETER);
}

