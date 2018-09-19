/**
 * System tick abstraction
 *
 * USB Midi-Fader
 *
 * Kevin Cuzner
 */

#include "systick.h"

#include "stm32f0xx.h"

#define SYSTICK_N_HANDLERS 8

static SystickHandler handlers[SYSTICK_N_HANDLERS];

void systick_init(void)
{
    SysTick_Config(8000);
}

void systick_subscribe(SystickHandler handler)
{
    for (uint8_t i = 0; i < SYSTICK_N_HANDLERS; i++)
    {
        if (handlers[i])
            continue;
        handlers[i] = handler;
        return;
    }
}

void SysTick_Handler(void)
{
    for (uint8_t i = 0; i < SYSTICK_N_HANDLERS; i++)
    {
        if (handlers[i])
        {
            handlers[i]();
        }
    }
}

