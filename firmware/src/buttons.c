/**
 * USB Midi-Fader
 *
 * Buttons and LEDs
 *
 * Kevin Cuzner
 */

#include "buttons.h"

#include "stm32f0xx.h"

/**
 * This uses the SPI peripheral to operate the shift registers that attach to
 * the buttons and LEDs on each midi-fader board.
 *
 * There can be up to four shift register pairs attached to one controller
 * board. The last shift register pair is noted by being all zeros when no
 * buttons are pressed. The number of channels in the system is determined by
 * how many of those bytes are non-zero. This number can grow and shrink
 * dynamically, even though we don't explicitly support hot-plugging of the
 * additional fader modules (in fact, I think it might cause weird behavior in
 * this module when they are being plugged in).
 */

/**
 * Receive buffer for the button status
 */
static uint8_t buttons_status[4];
/**
 * Transmit buffer for the LED status
 */
static uint8_t leds_status[4];

/**
 * Toggles the LCLK pin
 */
static void buttons_lclk_pulse(void)
{
    // The STP08D05 can handle a 20ns pulse
    // The 74HCT165 can handle a pulse between 100ns and 20ns
    //
    // This microcontroller is not operating at 50MHz, so the maximum
    // toggle frequency is probably somewhere quite a bit lower than 20ns
    // (though I suspect it can reach 100ns (10MHz)).
    GPIOB->BSRR = GPIO_BSRR_BS_0;
    GPIOB->BSRR = GPIO_BSRR_BR_0;
}

/**
 * Begins a DMA-driven transfer on the SPI
 *
 * The DMA channel must be configured in advance
 */
static void buttons_begin_transfer(void)
{
    // Set up the DMA transfer sizes
    DMA1_Channel2->CNDTR = 4;
    DMA1_Channel3->CNDTR = 4;

    // See section 25.8.9 in the reference manual for this procedure
    // Step 1: Enable RX DMAEN
    SPI1->CR2 |= SPI_CR2_RXDMAEN;
    // Step 2: Enable DMA Streams
    DMA1_Channel2->CCR |= DMA_CCR_EN;
    DMA1_Channel3->CCR |= DMA_CCR_EN;
    // Step 3: Enable TX DMAEN
    SPI1->CR2 |= SPI_CR2_TXDMAEN;
    // Step 4: Start the transfer
    SPI1->CR1 |= SPI_CR1_SPE;
}

/**
 * Concludes a DMA-driven transfer on the SPI
 *
 * This should only be called once the receive transfer (DMA_Channel3)
 * is completed.
 *
 * Also pulses the LCLK both to prepare the buttons for the next transfer
 * and to display the LED values from this transfer
 */
static void buttons_end_transfer(void)
{
    // See section 25.8.9 in the reference manual for this procedure
    // Step 1: Disable the DMA streams
    DMA1_Channel3->CCR &= ~DMA_CCR_EN;
    // Step 2: Follow the SPI disable procedure
    // Step 2.1: Wait for the transmit fifo to be empty
    while (SPI1->SR & SPI_SR_FTLVL_Msk) { }
    // Step 2.2: Wait for busy to clear
    while (SPI1->SR & SPI_SR_BSY) { }
    // Step 2.3: Disable the SPI
    SPI1->CR1 &= ~SPI_CR1_SPE;
    // Step 2.4: Wait for the receive buffer to clear
    while (SPI1->SR & SPI_SR_FRLVL_Msk) { }
    //TODO: I suspect that I should keep RX enabled until this point, just in case
    DMA1_Channel2->CCR &= ~DMA_CCR_EN;
    // Step 3: Clear the DMAEN bits
    SPI1->CR2 &= ~(SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN);

    // Pulse the LCLK to latch everything
    buttons_lclk_pulse();

    // Set OE# low (note that it is kept low normally except right at the
    // start)
    GPIOB->BSRR = GPIO_BSRR_BR_1;
}

void buttons_init(void)
{
    // Enable SPI and GPIOB
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

    // Enable DMA
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    // PB0, PB1, PB3, PB4, and PB5 are the button/led pins
    GPIOB->MODER &= ~(GPIO_MODER_MODER0_Msk | GPIO_MODER_MODER1_Msk |
            GPIO_MODER_MODER3_Msk | GPIO_MODER_MODER4_Msk |
            GPIO_MODER_MODER5_Msk);

    // Configure latch clock and OE pins
    //
    // LCLK: PB0 (starts low)
    // OE#: PB1 (starts high)
    GPIOB->BSRR = GPIO_BSRR_BR_0 | GPIO_BSRR_BS_1;
    GPIOB->OTYPER &= ~(GPIO_OTYPER_OT_0 | GPIO_OTYPER_OT_1);
    GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR0_0 | GPIO_OSPEEDR_OSPEEDR0_1 |
        GPIO_OSPEEDR_OSPEEDR1_0 | GPIO_OSPEEDR_OSPEEDR1_1;
    GPIOB->MODER |= GPIO_MODER_MODER0_0 | GPIO_MODER_MODER1_0;

    // Configure SPI pins
    //
    // AF0 of PB3, PB4, and PB5
    GPIOB->MODER |= GPIO_MODER_MODER3_1 | GPIO_MODER_MODER4_1 |
        GPIO_MODER_MODER5_1;
    GPIOB->AFR[0] &= ~(GPIO_AFRL_AFSEL3_Msk | GPIO_AFRL_AFSEL4_Msk |
            GPIO_AFRL_AFSEL5_Msk);

    // Configure the SPI
    //
    SPI1->CR1 = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_LSBFIRST | SPI_CR1_MSTR;
    SPI1->CR2 = SPI_CR2_FRXTH | SPI_CR2_DS_0 | SPI_CR2_DS_1 | SPI_CR2_DS_2;

    //DMA Channel 2 handles SPI1_RX
    DMA1_Channel2->CPAR = (uint32_t)(&SPI1->DR);
    DMA1_Channel2->CMAR = (uint32_t)(buttons_status);
    // Enable the transfer complete interrupt, since this completes last
    // once we've sent everything.
    DMA1_Channel2->CCR = DMA_CCR_MINC | DMA_CCR_TEIE | DMA_CCR_TCIE;

    //DMA Channel 3 handles SPI1_TX
    DMA1_Channel3->CPAR = (uint32_t)(&SPI1->DR);
    DMA1_Channel3->CMAR = (uint32_t)(leds_status);
    // Only enable the transfer error interrupt
    DMA1_Channel3->CCR = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_TEIE;

    NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);

    // Begin the transfer
    buttons_begin_transfer();
}

uint8_t buttons_get_count(void)
{
    uint8_t i;
    for (i = 1; i < 5; i++)
    {
        if (!buttons_status[i-1])
            break;
    }
    // buttons always come in pairs
    return i * 2;
}

uint8_t buttons_read(void)
{
    uint8_t compressed = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        compressed >>= 2;
        compressed |= (buttons_status[i] & 0x3) << 6;
    }
    return compressed;
}

void buttons_write_leds(uint8_t leds)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        leds_status[i] = leds & 0x3;
        leds >>= 2;
    }
}

void SPI1_IRQHandler(void)
{
}

static uint32_t transfers = 0;

void DMA1_Channel2_3_IRQHandler(void)
{
    if (DMA1->ISR & DMA_ISR_TEIF2)
    {
        // Channel 2 DMA error, just clear it for now
        DMA1->IFCR = DMA_IFCR_CTEIF2;
    }
    if (DMA1->ISR & DMA_ISR_TCIF2)
    {
        transfers++;
        // Channel 2 transfer complete. We have finished transacting all the
        // bytes in the buffer.
        buttons_end_transfer();
        buttons_begin_transfer();
    }
    if (DMA1->ISR & DMA_ISR_TEIF3)
    {
        // Channel 3 DMA error, just clear it for now
        DMA1->IFCR = DMA_IFCR_CTEIF3;
    }

    DMA1->IFCR = DMA_IFCR_CGIF2 | DMA_IFCR_CGIF3;
}

