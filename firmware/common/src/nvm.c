/**
 * Common NVM functions
 *
 * STM32F0xx
 *
 * Kevin Cuzner
 */

#include "nvm.h"

#include "stm32f0xx.h"
#include "atomic.h"

/**
 * Certain functions, such as flash write, are easier to do if the code is
 * executed from the RAM. This decoration relocates the function there and
 * prevents any inlining that might otherwise move the function to flash.
 */
#define _RAM __attribute__((section (".data#"), noinline))

/**
 * Unlocks the flash for write operations
 */
static void nvm_unlock_flash(void)
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
static void nvm_lock(void)
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
 * err: Error instance
 */
static _RAM void nvm_flash_do_write(uint16_t *addr, uint16_t data, Error err)
{
    if (ERROR_IS_FATAL(err))
        return;

    //half-word program operation
    FLASH->CR |= FLASH_CR_PG;
    *addr = data;
    //wait for completion
    while (FLASH->SR & FLASH_SR_BSY) { }
    if (FLASH->SR & FLASH_SR_EOP)
    {
        FLASH->SR = FLASH_SR_EOP;
        if (*addr != data)
            ERROR_SET(err, NVM_ERR_VERIFY);
    }
    else
    {
        if (FLASH->SR & FLASH_SR_WRPRTERR)
        {
            ERROR_SET(err, NVM_ERR_WRITEPROT);
        }
        else if (FLASH->SR & FLASH_SR_PGERR)
        {
            ERROR_SET(err, NVM_ERR_PROGRAM);
        }

        FLASH->SR = FLASH_SR_WRPRTERR | FLASH_SR_PGERR;
    }
    FLASH->CR &= ~FLASH_CR_PG;
}

/**
 * Wrapper function that prepares and performs a flash write
 *
 * addr: Halfword-aligned address to write
 * data: Halfword to write
 */
void nvm_flash_write(uint16_t *addr, uint16_t data, Error err)
{
    if (ERROR_IS_FATAL(err))
        return;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        nvm_unlock_flash();
        nvm_flash_do_write(addr, data, err);
        nvm_lock();
    }
}

/**
 * RAM-located function which performs the page erase. Pages are 1KB in
 * size.
 *
 * pageaddr: Address located within the 1KB page to be erased.
 */
static _RAM void nvm_flash_do_erase_page(uint16_t *pageaddr, Error err)
{
    if (ERROR_IS_FATAL(err))
        return;

    //page erase operation
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = (uint32_t)(pageaddr);
    FLASH->CR |= FLASH_CR_STRT;
    //wait for completion
    while (FLASH->SR & FLASH_SR_BSY) { }
    if (FLASH->SR & FLASH_SR_EOP)
    {
        FLASH->SR = FLASH_SR_EOP;
    }
    else
    {
        if (FLASH->SR & FLASH_SR_WRPRTERR)
        {
            ERROR_SET(err, NVM_ERR_ERASE_WRITEPROT);
        }
        else if (FLASH->SR & FLASH_SR_PGERR)
        {
            ERROR_SET(err, NVM_ERR_ERASE_PROGRAM);
        }

        FLASH->SR = FLASH_SR_WRPRTERR | FLASH_SR_PGERR;
    }
    FLASH->CR &= ~FLASH_CR_PER;
}

/**
 * Wrapper function that prepares and performs a flash erase
 *
 * pageaddr: Address within the page to erase
 */
void nvm_flash_erase_page(uint16_t *pageaddr, Error err)
{
    if (ERROR_IS_FATAL(err))
        return;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        nvm_unlock_flash();
        nvm_flash_do_erase_page(pageaddr, err);
        nvm_lock();
    }
}

