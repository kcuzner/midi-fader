/**
 * STM32L052X8 linker script
 *
 * Copyright 2017 Kevin Cuzner
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(Reset_Handler)

/**
 * A word about the PMA
 *
 * On the STM32F0xxx the PMA is structured as 512 16-bit words with no special
 * access restrictions other than 32-bit accesses are disallowed. It must be
 * accessed by byte or half-word.
 *
 * The purpose of this linker script is to enable gcc to manage usage
 * of symbols located inside the PMA and produce linker errors when statically
 * allocated symbols overrun the area. In order to accomplish this, this linker
 * script declares the PMA to be 1024 bytes long. In the USB driver, macros are
 * provided which will translate a linker-generated address into a "local
 * address" which can be used with the USB registers and an "application
 * address" which can be used by normal code to make word-accesses into the
 * memory.
 *
 * Direct usage of symbols allocated in the PMA may cause unexpected behavior.
 */

_flash_origin = 0x08000000;
_flash_length = 32K;
_storage_size = 2K;

MEMORY
{
    FLASH (RX) : ORIGIN = _flash_origin, LENGTH = _flash_length - _storage_size
    FLASH_STORAGE (RX) : ORIGIN = (_flash_origin + _flash_length) - _storage_size, LENGTH = _flash_length
    RAM (W!RX)  : ORIGIN = 0x20000000, LENGTH = 6K
    PMA (W)  : ORIGIN = 0x40006000, LENGTH = 1024 /* 512 x 16bit */
}

/**
 * Define the top of the stack as the end of RAM
 */
_heap_size_min = 256;
_stack_size_min = 512; /* On a system with 64K, 8K was used */
_estack = (0x20000000+6K);

SECTIONS
{
    /* General code */
    .text :
    {
        _flash_start = .;
        . = ALIGN(4);
        /* At beginning of flash is:
         *
         * Required:
         * 0x0000 Initial stack pointer
         * 0x0004 Reset Handler
         *
         * Optional:
         * 0x0008 and beyond: NVIC ISR Table
         */
        KEEP(*(.isr_vector))
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)

        /* C startup support */
        /* TODO: Convert to -nostartfiles for maximum DIY */
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
    } > FLASH

    /* Additional C++ Support sections */
    .preinit_array :
    {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    } > FLASH
    .init_array :
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
    } > FLASH
    .fini_array :
    {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } > FLASH

    /* Load address of .data in flash for startup code */
    _sidata = LOADADDR(.data);

    /* Initialized data */
    .data :
    {
        . = ALIGN(4);
        _sdata = .; /* Start of data in RAM */
        *(.data);
        *(.data*);

        . = ALIGN(4);
        _edata = .; /* End of data in RAM */
    } > RAM AT > FLASH

    /* Storage located at the end of flash */
    .storage :
    {
        . = ALIGN(4);
        _sstorage = .;
        KEEP(*(.storage))
        KEEP(*(.storage*))
        . = _sstorage + _storage_size / 2;
        FILL(0xFF)
        _mstorage = .;
        . = _sstorage + _storage_size;
        _estorage = .;
    } > FLASH_STORAGE

    /* Zeroed data */
    . = ALIGN(4);
    .bss :
    {
        _sbss = .; /* Start of bss in RAM */
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        . = ALIGN(4);
        _ebss = .; /* End of bss in RAM */
        __bss_end__ = _ebss;
    } > RAM

    /* Fake stack section to ensure that sufficient space is left */
    ._user_heap_stack :
    {
        . = ALIGN(4);
        . = . + _stack_size_min;
        . = . + _heap_size_min;
        . = ALIGN(4);
    } > RAM

    /* USB/CAN Packet Memory Area (PMA) */
    .pma :
    {
        _pma_start = .; /* Start of PMA in real memory space */
        . = ALIGN(2);
        *(.pma)
        *(.pma*)
        . = ALIGN(2);
        _pma_end = .; /* End of PMA in PMA space */
    } > PMA

    /* Remove information from the standard libraries */
    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }

    PROVIDE_HIDDEN (__exidx_start = .);
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    }
    PROVIDE_HIDDEN (__exidx_end = .);
}

