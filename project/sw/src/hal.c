#include "../inc/hal.h"

// Addresses of the peripherals in the memory map

#define IO_BASE         0x40000000u
#define LED_ADDR        (IO_BASE + 0x0)
#define TIMER_ADDR      (IO_BASE + 0x4)
#define UART_ADDR       (IO_BASE + 0x8)
#define BUTTONS_ADDR    (IO_BASE + 0xc)
#define SPI_BASE_ADDR   (IO_BASE + 0x10)
#define SPI_CNTRL_ADDR  (SPI_BASE_ADDR + 0x0)
#define SPI_STATUS_ADDR (SPI_BASE_ADDR + 0x4)
#define SPI_TX_DAT_ADDR (SPI_BASE_ADDR + 0x8)
#define SPI_RX_DAT_ADDR (SPI_BASE_ADDR + 0xC)

/**
 * Delay for a given number of cycles by inserting nops.
 * @param cycles The number of fetch cycles to delay
 */
void delay_cycles (unsigned int cycles) 
{
    while (cycles--) {
        asm("nop");
    }   
}

/**
 * Set the LEDs to a given value.
 * For the CMOD A7 there is only 2 LEDs.
 * @param value The value to set the LEDs to
 */
void set_leds (unsigned int value)
{
    volatile unsigned int *leds = (volatile unsigned int *)LED_ADDR;
    *leds = value;
}

/**
 * Reset the timer counter by parsing a value of 1.
 */
void set_counter() {
    volatile unsigned int *timer = (volatile unsigned int *)TIMER_ADDR;
    *timer = 1;
}

/**
 * Read the current value of the timer counter.
 * @param value Pointer to store the read timer value
 */
void read_timer(unsigned int *value) {
    volatile unsigned int *timer = (volatile unsigned int *)TIMER_ADDR;
    *value = *timer;
}

/**
 * Write a byte to the UART.
 * @param c The byte to write
 */
void uart_write_byte(unsigned int c) {
    volatile unsigned int *uart = (volatile unsigned int *)UART_ADDR;
    while (*uart & (1u << 8)) { }     /* wait while TX busy */
    *uart = c;
}

/**
 * Read the current value from the XADC.
 * @param value Pointer to store the read XADC value
 */
void read_xadc(unsigned int *value) {
    volatile unsigned int *xadc = (volatile unsigned int *)XADC_ADDR;
    *value = *xadc;
}

/**
 * Write a value to a SPI register.
 * @param addr The address of the SPI register to write to (see hal.h for register addresses)
 * @param value The value to write to the register
 */
void spi_write_reg(unsigned int addr, unsigned int value)
{
    *(volatile unsigned int*)addr = value;
}

/**
 * Read a value from a SPI register.
 * @param addr The address of the SPI register to read from (see hal.h for register addresses)
 * @return The value read from the register
 */
unsigned int spi_read_reg(unsigned int addr)
{
    return *(volatile unsigned int*)addr;
}

/**
 * Configure the SPI controller with the given clock mode, data mode, and clock divider.
 * @param clk_mode The clock mode (0-3) to set for the SPI controller
 * @param data_mode The data mode (0 or 1) to set for the SPI controller
 * @param div The clock divider (0-128) to set for the SPI controller
 */
void spi_configure(unsigned int clk_mode,
              unsigned int data_mode,
              unsigned int div)
{
    unsigned int ctrl = 0;

    ctrl |= (clk_mode & 0x3) << 1;
    ctrl |= (data_mode & 0x1) << 3;
    ctrl |= (div & 0xFF) << 8;

    spi_write_reg(SPI_CNTRL_ADDR, ctrl);
}

/**
 * Set the specified slave(s) by writing to the SPI control register.
 * @param slave A bitmask of the slave(s)
 */
void spi_set_slave(unsigned int slave)
{
    unsigned int ctrl = spi_read_reg(SPI_CNTRL_ADDR);
    ctrl &= ~(0xFFu << 16);
    ctrl |= (slave & 0xFF) << 16;
    spi_write_reg(SPI_CNTRL_ADDR, ctrl);
}

/**
 * Remove the specified slave(s) by writing to the SPI control register.
 * @param slave A bitmask of the slave(s)
 */
void spi_remove_slave(unsigned int slave)
{
    unsigned int ctrl = spi_read_reg(SPI_CNTRL_ADDR);
    ctrl &= ~(0xFFu << 16);
    ctrl |= ((~slave) & 0xFF) << 16;
    spi_write_reg(SPI_CNTRL_ADDR, ctrl);
}