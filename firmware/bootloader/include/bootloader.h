/**
 * LED Wristwatch Bootloader
 *
 * Kevin Cuzner
 */

#ifndef _BOOTLOADER_H_
#define _BOOTLOADER_H_

#define FLASH_LOWER_BOUND 0x08002000
#define FLASH_UPPER_BOUND 0x080077FF

/**
 * Initializes the bootloader component
 */
void bootloader_init(void);

/**
 * Runs the bootloader component
 */
void bootloader_run(void);

/**
 * IRQ Handler for the bootloader
 */
void Bootloader_IRQHandler(void);

#endif //_BOOTLOADER_H_

