/**
 * USB Midi-Fader
 *
 * Kevin Cuzner
 *
 * Oscillator control
 */

#include "osc.h"
#include "stm32f0xx.h"

static OscChangeCallback change_callbacks[OSC_MAX_CALLBACKS];
static uint8_t next_change_callback = 0;

static void osc_run_callbacks(void)
{
    SystemCoreClockUpdate();
    for (uint8_t i = 0; i < next_change_callback; i++)
        change_callbacks[i]();
}

void osc_request_hsi8(void)
{
    //turn on HSI8 and switch the processor clock
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) { }
    RCC->CFGR ^= (RCC->CFGR ^ RCC_CFGR_SW_HSI) & RCC_CFGR_SW;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) { }
    //turn off the other clocks, except HSI48 since it could be in use by USB
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON);

    osc_run_callbacks();
}

void osc_request_hsi8_pll(uint8_t prediv, uint8_t mul)
{
    //request the standard HSI8 clock
    osc_request_hsi8();

    //The PLL was disabled when HSI8 was enabled, so let's set it up
    RCC->CFGR ^= (RCC->CFGR ^ ((mul << RCC_CFGR_PLLMUL_Pos) & RCC_CFGR_PLLMUL)) & RCC_CFGR_PLLMUL;
    RCC->CFGR2 ^= (RCC->CFGR2 ^ ((prediv << RCC_CFGR2_PREDIV_Pos) & RCC_CFGR2_PREDIV)) & RCC_CFGR2_PREDIV;
    RCC->CFGR ^= (RCC->CFGR ^ RCC_CFGR_PLLSRC_HSI_PREDIV) & RCC_CFGR_PLLSRC;
    //Turn on the PLL and switch the processor clock
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) { }
    RCC->CFGR ^= (RCC->CFGR & RCC_CFGR_SW_PLL) & RCC_CFGR_SW;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }
    //Turn off the HSE, but keep the HSI8 turned on
    RCC->CR &= ~(RCC_CR_HSEON);

    osc_run_callbacks();
}

void osc_start_hsi48(void)
{
    //Turn on HSI48
    RCC->CR2 |= RCC_CR2_HSI48ON;
    while (!(RCC->CR2 & RCC_CR2_HSI48RDY)) { }
}

void osc_request_hsi48(void)
{
    //Turn on HSI48 and switch the processor clock
    osc_start_hsi48();
    RCC->CFGR &= (RCC->CFGR & RCC_CFGR_SW_HSI48) & RCC_CFGR_SW;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI48) { }
    //turn off the other clocks
    RCC->CR ^= ~(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_HSION);

    osc_run_callbacks();
}

void osc_add_callback(OscChangeCallback fn)
{
    change_callbacks[next_change_callback++] = fn;
}

