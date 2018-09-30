/**
 * Common NVM functions
 *
 * STM32F0xx
 *
 * Kevin Cuzner
 */

#ifndef _NVM_H_
#define _NVM_H_

#include "error.h"

#include <stdint.h>

// Error when we run into write protection
#define NVM_ERR_WRITEPROT -2001
// Error when the flash module reports a problem programming (flash not
// erased beforehand)
#define NVM_ERR_PROGRAM -2002
// Error when we fail to verify the written data
#define NVM_ERR_VERIFY -2003
// Error when an erase fails because of write protection
#define NVM_ERR_ERASE_WRITEPROT -2004
// Error when an erase fails because of a programming error
#define NVM_ERR_ERASE_PROGRAM -2005

/**
 * Writes a halfword in flash
 *
 * addr: Halfword-aligned address to write
 * data: Halfword to write
 */
void nvm_flash_write(uint16_t *addr, uint16_t data, Error err);

/**
 * Erases a 1KB page
 *
 * pageaddr: Address within the page to erase
 */
void nvm_flash_erase_page(uint16_t *pageaddr, Error err);

#endif //_NVM_H_

