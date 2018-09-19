/**
 * USB Midi-Fader
 *
 * Fader implementation
 *
 * Kevin Cuzner
 */

#include "fader.h"

#include "stm32f0xx.h"
#include "osc.h"

/**
 * In reality, this is a really crappy abstraction around the ADC. It is not
 * really transferrable to any of my other projects, which I dislike. But, in
 * the interest of time I am implementing it this way.
 *
 * The basic premise is that this will simply place the ADC in continuous
 * conversion mode, scanning the 8 channels. At the end of a sequence, it will
 * transfer the scanned data to a buffer visible to the rest of the program.
 * It will also call a hook function which I may or may not use. Real simple,
 * real quick.
 */

#define FADER_CHANNELS 8

/**
 * A basic averaging filter is implemented by simply averaging every Nth
 * element of the fader data. This should be a power of two for fast
 * division.
 */
#define FADER_AVERAGES 16

/**
 * Target for the DMA, contains the latest fader data
 */
static uint16_t fader_data[FADER_CHANNELS * FADER_AVERAGES];

void fader_init(void)
{
    // Enable HSI14 for the ADC clock
    osc_start_hsi14();

    // Enable ADC and PortA
    RCC->APB2ENR |= RCC_APB2ENR_ADCEN;
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    // Enable DMA
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    // Set up the ADC pins
    GPIOA->MODER |= GPIO_MODER_MODER0_0 | GPIO_MODER_MODER0_1 |
        GPIO_MODER_MODER1_0 | GPIO_MODER_MODER1_1 |
        GPIO_MODER_MODER2_0 | GPIO_MODER_MODER2_1 |
        GPIO_MODER_MODER3_0 | GPIO_MODER_MODER3_1 |
        GPIO_MODER_MODER4_0 | GPIO_MODER_MODER4_1 |
        GPIO_MODER_MODER5_0 | GPIO_MODER_MODER5_1 |
        GPIO_MODER_MODER6_0 | GPIO_MODER_MODER6_1 |
        GPIO_MODER_MODER7_0 | GPIO_MODER_MODER7_1;

    //Perform ADC calibration
    if (ADC1->CR & ADC_CR_ADEN)
    {
        ADC1->CR |= ADC_CR_ADDIS;
        while (ADC1->CR & ADC_CR_ADEN) { }
    }
    ADC1->CR |= ADC_CR_ADCAL;
    while (ADC1->CR & ADC_CR_ADCAL) { }

    //Enable the ADC
    ADC1->CR |= ADC_CR_ADEN;
    while (!(ADC1->ISR & ADC_ISR_ADRDY)) { }
    ADC1->ISR = ADC_ISR_ADRDY;

    // Prepare for sequenced conversion, continuously
    //
    // The DMA is set up in circular mode so that we just keep filling the
    // buffer as fast as possible
    ADC1->CFGR1 = ADC_CFGR1_CONT | ADC_CFGR1_DMAEN | ADC_CFGR1_DMACFG;
    ADC1->CHSELR = ADC_CHSELR_CHSEL0 | ADC_CHSELR_CHSEL1 |
        ADC_CHSELR_CHSEL2 | ADC_CHSELR_CHSEL3 | ADC_CHSELR_CHSEL4 |
        ADC_CHSELR_CHSEL5 | ADC_CHSELR_CHSEL6 | ADC_CHSELR_CHSEL7;
    ADC1->SMPR = ADC_SMPR_SMP_0 | ADC_SMPR_SMP_1 | ADC_SMPR_SMP_2;
    DMA1_Channel1->CPAR = (uint32_t)(&ADC1->DR);
    DMA1_Channel1->CMAR = (uint32_t)(fader_data);
    DMA1_Channel1->CNDTR = sizeof(fader_data) / sizeof(fader_data[0]);
    DMA1_Channel1->CCR = DMA_CCR_MINC | DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0 |
        DMA_CCR_TEIE | DMA_CCR_CIRC;
    DMA1_Channel1->CCR |= DMA_CCR_EN;

    ADC1->IER |= ADC_IER_EOSEQIE;
    ADC1->CR |= ADC_CR_ADSTART;

    NVIC_EnableIRQ(ADC1_COMP_IRQn);
}

uint16_t fader_get_value(uint8_t channel)
{
    if (channel > FADER_CHANNELS - 1)
        return 0;
    uint32_t accumulator = 0;
    for (uint32_t i = channel; i < sizeof(fader_data)/sizeof(fader_data[0]); i += FADER_CHANNELS)
    {
        accumulator += fader_data[i];
    }
    accumulator /= FADER_AVERAGES;
    return accumulator;
}

static uint32_t conversions = 0;

void ADC1_IRQHandler(void)
{
    if (ADC1->ISR & ADC_ISR_EOSEQ)
        conversions++;
    ADC1->ISR = ADC1->ISR;
}

