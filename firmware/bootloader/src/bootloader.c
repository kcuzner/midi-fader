/**
 * LED Wristwatch Bootloader
 *
 * Kevin Cuzner
 */

#include "bootloader.h"

#include "stm32f0xx.h"
#include "nvm.h"
#include "usb_hid.h"
#include "storage.h"
#include "error.h"
#include "_gen_storage.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/**
 * Places a symbol into the reserved RAM section. This RAM is not
 * initialized and must be manually initialized before use.
 */
#define RSVD_SECTION ".rsvd.data,\"aw\",%nobits//"
#define _RSVD __attribute__((used, section(RSVD_SECTION)))

/**
 * Mask for RCC_CSR defining which bits may trigger an entry into bootloader mode:
 * - Any watchdog reset
 * - Any soft reset
 * - A pin reset (aka manual reset)
 */
#define BOOTLOADER_RCC_CSR_ENTRY_MASK (RCC_CSR_WWDGRSTF | RCC_CSR_IWDGRSTF | RCC_CSR_SFTRSTF | RCC_CSR_PINRSTF)

/**
 * Magic code value to make the bootloader ignore any of the entry bits set in
 * RCC_CSR and skip to the user program anyway, if a valid program start value
 * has been programmed.
 */
#define BOOTLOADER_MAGIC_SKIP 0x3C65A95A

#define BOOTLOADER_ERR_FSM -3001
#define BOOTLOADER_ERR_COMMAND -3002
#define BOOTLOADER_ERR_BAD_ADDR -3003
#define BOOTLOADER_ERR_BAD_CRC32 -3004
#define BOOTLOADER_ERR_WRITE -3005
#define BOOTLOADER_ERR_SHORT -3006
#define BOOTLOADER_ERR_VERIFY -3007

#define CMD_RESET 0x00000000
#define CMD_PROG  0x00000080
#define CMD_READ  0x00000040
#define CMD_EXIT  0x000000C3
#define CMD_ABORT 0x0000003E

static union {
    uint32_t buffer[16];
    struct {
        uint32_t last_command;
        uint32_t status;
        uint32_t crc32_lower;
        uint32_t crc32_upper;
        uint8_t data[48];
    };
} in_report;

union {
    uint32_t buffer[16];
    uint16_t buffer_hw[32];
    struct {
        uint32_t command;
        uint16_t *address;
        uint32_t crc32_lower;
        uint32_t crc32_upper;
    };
} out_report;

static const USBTransferData in_report_data = { &in_report, sizeof(in_report) };
static const USBTransferData out_report_data = { &out_report, sizeof(out_report) };

_Static_assert(sizeof(in_report) == 64, "Bootloader IN report is improperly sized. Fix this!");
_Static_assert(sizeof(out_report) == 64, "Bootloader OUT report is improperly sized. Fix this!");

typedef enum { EV_ANY, EV_CONFIGURED, EV_HID_OUT, EV_HID_IN, EV_HID_OUT_SHORT } BootloaderEvent;
typedef enum { ST_ANY, ST_RESET, ST_STATUS, ST_LPROG, ST_UPROG, ST_LREAD, ST_UREAD } BootloaderState;

typedef BootloaderState (*BootloaderFsmFn)(BootloaderEvent);

typedef struct {
    BootloaderState state;
    BootloaderEvent ev;
    BootloaderFsmFn fn;
} BootloaderFsmEntry;

static volatile struct {
    uint16_t *address;
    uint32_t crc32_lower;
    uint32_t crc32_upper;
    BootloaderState next_state; //used when sending a status report
} bootloader_state;

/**
 * Performs the sequence which will reset the device into the user program. Note
 * that if the EEPROM write performed during this function fails, it will erase
 * the user_vector_value to prevent booting into a bad program.
 */
static void bootloader_reset_into_user_prog(Error err)
{
    //at this point, if a reset occurs we will still enter bootloader mode

    //write the magic bootloader-skip value so we can perform a reset
    uint32_t buf = BOOTLOADER_MAGIC_SKIP;
    storage_write(STORAGE_BOOTLOADER_MAGIC, &buf, sizeof(buf), err);
    if (ERROR_IS_FATAL(err))
    {
        ERROR_INST(err_temp);
        buf = 0;
        storage_write(STORAGE_BOOTLOADER_USER_VTOR, &buf, sizeof(buf), &err_temp);
        return;
    }
    //at this point, if a reset occurs we will not enter bootloader mode. We have entered the danger zone.

    //perform the system reset
    NVIC_SystemReset();
}

/**
 * Sends a status report, starting an IN-OUT sequence
 */
static BootloaderState bootloader_send_status(BootloaderState next)
{
    bootloader_state.next_state = next;
    usb_hid_send(&in_report_data);

    return ST_STATUS;
}

/**
 * Sends a status report, queueing up a state change to ST_RESET
 */
static BootloaderState bootloader_enter_reset(void)
{
    memset(in_report.buffer, 0, sizeof(in_report));
    in_report.last_command = CMD_RESET;
    return bootloader_send_status(ST_RESET);
}

/**
 * Sends a status report, queueing up a state change to ST_LPROG
 */
static BootloaderState bootloader_enter_prog(void)
{
    ERROR_INST(err);

    bootloader_state.address = out_report.address;
    bootloader_state.crc32_lower = out_report.crc32_lower;
    bootloader_state.crc32_upper = out_report.crc32_upper;

    memset(in_report.buffer, 0, sizeof(in_report));
    in_report.last_command = CMD_PROG;

    //if needed, erase the previous entry point
    uint32_t storage_buf;
    size_t len = sizeof(storage_buf);
    storage_read(STORAGE_BOOTLOADER_USER_VTOR, &storage_buf, &len, &err);
    if (ERROR_IS_FATAL(&err))
        goto done;
    if (storage_buf)
    {
        storage_buf = 0;
        storage_write(STORAGE_BOOTLOADER_USER_VTOR, &storage_buf, sizeof(storage_buf), &err);
    }
    if (ERROR_IS_FATAL(&err))
        goto done;

    uint32_t address = (uint32_t)bootloader_state.address;
    //determine if the address is aligned and in range
    if ((address & 0x7F) || (address < FLASH_LOWER_BOUND) || (address > FLASH_UPPER_BOUND))
    {
        ERROR_SET(&err, BOOTLOADER_ERR_BAD_ADDR);
        in_report.status = BOOTLOADER_ERR_BAD_ADDR;
    }
    else
    {
        //erase the page
        nvm_flash_erase_page(bootloader_state.address, &err);
    }

done:
    if (ERROR_IS_FATAL(&err))
    {
        in_report.status = err;
        return bootloader_send_status(ST_RESET);
    }
    else
    {
        return bootloader_send_status(ST_LPROG);
    }
}

/**
 * Sends a status report, queueing up a state change to ST_LREAD
 */
static BootloaderState bootloader_enter_read(void)
{
    memset(in_report.buffer, 0, sizeof(in_report));
    in_report.last_command = CMD_READ;
    //TODO: CRC32
    //TODO: This can't use send_status because it creates an IN-IN-IN sequence
    return bootloader_send_status(ST_LREAD);
}

/**
 * Performs the bootloader exit sequence
 */
static BootloaderState bootloader_exit(void)
{
    ERROR_INST(err);

    memset(in_report.buffer, 0, sizeof(in_report));
    in_report.last_command = CMD_EXIT;
    //ensure we have somewhere to start the program
    if (!out_report.address)
    {
        ERROR_SET(&err, BOOTLOADER_ERR_BAD_ADDR);
        goto error;
    }

    //write the passed address
    uint32_t address = (uint32_t)out_report.address;
    if ((address < FLASH_LOWER_BOUND) || (address > FLASH_UPPER_BOUND))
    {
        ERROR_SET(&err, BOOTLOADER_ERR_BAD_ADDR);
        goto error;
    }
    storage_write(STORAGE_BOOTLOADER_USER_VTOR, &address, sizeof(address), &err);
    if (ERROR_IS_FATAL(&err))
    {
        goto error;
    }

    //perform the system reset
    bootloader_reset_into_user_prog(&err);
    if (ERROR_IS_FATAL(&err))
    {
        goto error;
    }

    //if we somehow don't reset, return the bootloader to the reset state
    return ST_RESET;

error:
    in_report.status = err;
    return bootloader_send_status(ST_RESET);
}

static BootloaderState bootloader_abort(void)
{
    ERROR_INST(err);
    memset(in_report.buffer, 0, sizeof(in_report));
    in_report.last_command = CMD_ABORT;

    uint32_t user_vtor = 0;
    size_t len = sizeof(user_vtor);
    storage_read(STORAGE_BOOTLOADER_USER_VTOR, &user_vtor, &len, &err);
    if (ERROR_IS_FATAL(&err))
    {
        goto error;
    }

    //perform the system reset
    bootloader_reset_into_user_prog(&err);
    if (ERROR_IS_FATAL(&err))
    {
        goto error;
    }

    //if we somehow don't reset, return the bootloader to the reset state
    return ST_RESET;

error:
    in_report.status = err;
    return bootloader_send_status(ST_RESET);
}

static BootloaderState bootloader_fsm_configured(BootloaderEvent ev)
{
    usb_hid_receive(&out_report_data);
    return ST_RESET;
}

static BootloaderState bootloader_fsm_reset(BootloaderEvent ev)
{
    switch (ev)
    {
    case EV_HID_OUT:
        if (out_report.command == CMD_RESET)
        {
            return bootloader_enter_reset();
        }
        else if (out_report.command == CMD_PROG)
        {
            return bootloader_enter_prog();
        }
        else if (out_report.command == CMD_READ)
        {
            bootloader_state.address = out_report.address;
            return bootloader_enter_read();
        }
        else if (out_report.command == CMD_EXIT)
        {
            return bootloader_exit();
        }
        else if (out_report.command == CMD_ABORT)
        {
            return bootloader_abort();
        }
        else
        {
            memset(in_report.buffer, 0, sizeof(in_report));
            in_report.last_command = out_report.command;
            in_report.status = BOOTLOADER_ERR_COMMAND;
            return bootloader_send_status(ST_RESET);
        }
        break;
    default:
        memset(in_report.buffer, 0, sizeof(in_report));
        in_report.status = BOOTLOADER_ERR_FSM;
        return bootloader_send_status(ST_RESET);
    }
}

static BootloaderState bootloader_fsm_status(BootloaderEvent ev)
{
    usb_hid_receive(&out_report_data);
    return bootloader_state.next_state;
}

/**
 * Shared function to writing an OUT report to flash
 */
static BootloaderState bootloader_fsm_program(bool upper, BootloaderEvent ev)
{
    uint32_t i, crc32, computed_crc32;
    uint16_t *address;
    ERROR_INST(err);

    if (ev != EV_HID_OUT)
    {
        ERROR_SET(&err, BOOTLOADER_ERR_FSM);
        goto error;
    }

    crc32 = upper ? bootloader_state.crc32_upper : bootloader_state.crc32_lower;

    //check the CRC32 (to avoid unexpected programming events)
    CRC->CR |= CRC_CR_RESET;
    for (i = 0; i < 16; i++)
    {
        CRC->DR = out_report.buffer[i];
    }
    computed_crc32 = ~CRC->DR; //invert result for zlib
    if (upper)
        in_report.crc32_upper = computed_crc32;
    else
        in_report.crc32_lower = computed_crc32;
    if (crc32 != computed_crc32)
    {
        ERROR_SET(&err, BOOTLOADER_ERR_BAD_CRC32);
        goto error;
    }

    //program the page
    //
    //NOTE: This was originally written for the STM32L0xx which programs by
    //half-page. In the interest of time, I'm just faking that here by
    //programming the 32 half-words in sequence since th STM32F0xx programs by
    //halfword only.
    address = upper ? &bootloader_state.address[32] : bootloader_state.address;
    for (i = 0; i < 32; i++)
    {
        nvm_flash_write(&address[i], out_report.buffer_hw[i], &err);
    }
    if (ERROR_IS_FATAL(&err))
    {
        goto error;
    }

    //verify the page
    for (i = 0; i < 16; i++)
    {
        if (address[i] != out_report.buffer[i])
        {
            ERROR_SET(&err, BOOTLOADER_ERR_VERIFY);
            goto error;
        }
    }

    in_report.status = err;
    return bootloader_send_status(upper ? ST_RESET : ST_UPROG);

error:
    in_report.status = err;
    return bootloader_send_status(ST_RESET);
}

/**
 * State executed when the lower page to program has been received
 */
static BootloaderState bootloader_fsm_lprog(BootloaderEvent ev)
{
    return bootloader_fsm_program(false, ev);
}

/**
 * State executed when the upper page to program has been received
 */
static BootloaderState bootloader_fsm_uprog(BootloaderEvent ev)
{
    return bootloader_fsm_program(true, ev);
}

static BootloaderState bootloader_fsm_short(BootloaderEvent ev)
{
    memset(in_report.buffer, 0, sizeof(in_report));
    in_report.status = BOOTLOADER_ERR_SHORT;
    return bootloader_send_status(ST_RESET);
}

static BootloaderState bootloader_fsm_error(BootloaderEvent ev)
{
    memset(in_report.buffer, 0, sizeof(in_report));
    in_report.status = BOOTLOADER_ERR_FSM;
    return bootloader_send_status(ST_RESET);
}

static const BootloaderFsmEntry fsm[] = {
    { ST_ANY, EV_CONFIGURED, &bootloader_fsm_configured },
    { ST_RESET, EV_HID_OUT, &bootloader_fsm_reset },
    { ST_STATUS, EV_HID_IN, &bootloader_fsm_status },
    { ST_LPROG, EV_HID_OUT, &bootloader_fsm_lprog },
    { ST_UPROG, EV_HID_OUT, &bootloader_fsm_uprog },
    { ST_ANY, EV_HID_OUT_SHORT, &bootloader_fsm_short },
    { ST_ANY, EV_ANY, &bootloader_fsm_error }
};
#define FSM_SIZE sizeof(fsm)/(sizeof(BootloaderFsmEntry))

/**
 * Returns 1 if the bootloader should start or 0 if the user program
 * might be allowed to start
 */
static uint8_t bootloader_check_reset_entry_conditions(void)
{
    uint32_t csr = RCC->CSR;
    // If any watchdog goes off or the soft reset occurs, the bootloader
    // should run
    if (csr & (RCC_CSR_WWDGRSTF | RCC_CSR_IWDGRSTF | RCC_CSR_SFTRSTF))
    {
        return 1;
    }
    // If there is a pin reset without a poweron reset, then the
    // bootloader should run (note that this depends on the bootloader
    // clearing the reset flags at boot)
    if (!(csr & RCC_CSR_PORRSTF) && (csr & RCC_CSR_PINRSTF))
    {
        return 1;
    }
    // If we get this far, the user program might run
    return 0;
}

static volatile _RSVD uint32_t bootloader_vtor;

extern uint32_t *g_pfnVectors;

void bootloader_init(void)
{
    ERROR_INST(err);

    bootloader_vtor = &g_pfnVectors;

    uint32_t user_vtor_value = 0;
    uint32_t magic = 0;
    size_t len = sizeof(user_vtor_value);
    storage_read(STORAGE_BOOTLOADER_USER_VTOR, &user_vtor_value, &len, &err);
    len = sizeof(magic);
    storage_read(STORAGE_BOOTLOADER_MAGIC, &magic, &len, &err);

    if (ERROR_IS_FATAL(&err))
        return;

    uint8_t reset_entry = bootloader_check_reset_entry_conditions();
    // Reset the entry flags
    RCC->CSR |= RCC_CSR_RMVF;

    //if the prog_start field is set and there are no entry bits set in the CSR (or the magic code is programmed appropriate), start the user program
    if (user_vtor_value &&
            (!reset_entry || (magic == BOOTLOADER_MAGIC_SKIP)))
    {
        if (magic)
        {
            magic = 0;
            storage_write(STORAGE_BOOTLOADER_MAGIC, &magic, sizeof(magic), &err);
        }
        __disable_irq();

        uint32_t *user_vtor = (uint32_t *)user_vtor_value;
        uint32_t sp = user_vtor[0];
        uint32_t pc = user_vtor[1];
        bootloader_vtor = user_vtor_value;
        __asm__ __volatile__("mov sp,%0\n\t"
                "bx %1\n\t"
                : /* no output */
                : "r" (sp), "r" (pc)
                : "sp");
        while (1) { }
    }
}

void bootloader_run(void)
{
    RCC->CSR |= RCC_CSR_RMVF;

    //Enable clocks for the bootloader
    RCC->AHBENR |= RCC_AHBENR_CRCEN;

    //zlib configuration: Reverse 32-bit in, reverse out, default polynomial and init value
    //Note that the result will need to be inverted
    CRC->CR = CRC_CR_REV_IN_0 | CRC_CR_REV_IN_1 | CRC_CR_REV_OUT;
}

/**
 * Handles bootloader state changes initiated by external events, such as
 * receiving HID events
 */
static void bootloader_tick(BootloaderEvent ev)
{
    static BootloaderState state = ST_RESET;
    uint32_t i;
    for (i = 0; i < FSM_SIZE; i++)
    {
        const BootloaderFsmEntry *entry = &fsm[i];
        if (entry->state == state || entry->state == ST_ANY)
        {
            if (entry->ev == ev || entry->ev == EV_ANY)
            {
                state = entry->fn(ev);
                return;
            }
        }
    }
}

void hook_usb_hid_configured(void)
{
    bootloader_tick(EV_CONFIGURED);
}

void hook_usb_hid_in_report_sent(const USBTransferData *report)
{
    bootloader_tick(EV_HID_IN);
}

uint32_t last_length;
void hook_usb_hid_out_report_received(const USBTransferData *report)
{
    if (report->len == 64)
    {
        bootloader_tick(EV_HID_OUT);
    }
    else
    {
        bootloader_tick(EV_HID_OUT_SHORT);
    }
}

/**
 * Entry point for all exceptions which passes off execution to the appropriate
 * handler. This adds some non-trivial overhead, but it does tail-call the
 * handler and I think it's about as minimal as you can get for emulating the
 * VTOR.
 */
void __attribute__((naked)) Bootloader_IRQHandler(void)
{
    __asm__ volatile (
            " movs r0, #4\n" // Determine whether to use PSP or MSP...
            " movs r1, lr\n"
            " tst r0, r1\n"
            " beq 1f\n"
            " mrs r0, psp\n"
            " b 2f\n"
          "1:\n"
            " mrs r0, msp\n"
          "2:\n"
            " ldr r0,=bootloader_vtor\n" // Read the fake VTOR into r0
            " ldr r0,[r0]\n"
            " ldr r1,=0xE000ED04\n" // Prepare to read the ICSR
            " ldr r1,[r1]\n" // Load the ICSR
            " mov r2,#63\n"  // Prepare to mask SCB_ICSC_VECTACTIVE (6 bits, Cortex-M0)
            " and r1, r2\n"  // Mask the ICSR, r1 now contains the vector number
            " lsl r1, #2\n"  // Multiply vector number by sizeof(function pointer)
            " add r0, r1\n"  // Apply the offset to the table base
            " ldr  r0,[r0]\n" // Read the function pointer value
            " bx r0\n" // Aaaannd branch!
            );
}

