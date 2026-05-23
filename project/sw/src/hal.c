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
 * @return The current value of the timer counter
 */
unsigned int read_timer() {
    volatile unsigned int *timer = (volatile unsigned int *)TIMER_ADDR;
    return *timer;
}

/**
 * Write a byte to the UART.
 * Warning: blocks until the UART is ready to transmit, so use with caution.
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
 * Write to a GPO pin to set it HIGH or LOW.
 * @param value The value to write to the GPO bit (HIGH or LOW, see hal.h)
 * @param IO_BIT The bit position of the GPO pin to write to (see hal.h for defines and @)
 */
void digital_write(unsigned int value, unsigned int IO_BIT) {
    // first read register
    volatile unsigned int *gpo_reg = (volatile unsigned int *)GPO_ADDR;
    const unsigned int mask = (1u << IO_BIT);
    unsigned int reg_val = *gpo_reg;
    // set or clear bit
    if (value) {
        reg_val |= mask;
    } else {
        reg_val &= ~mask;
    }
    *gpo_reg = reg_val;
}

/**
 * Read the value of a GPI pin.
 * @param IO_BIT The bit position of the GPI pin to read from (see hal.h for defines and @)
 * @return The value read from the GPI bit (HIGH or LOW)
 */
unsigned int digital_read(unsigned int IO_BIT) {
    volatile unsigned int *gpi_reg = (volatile unsigned int *)GPI_ADDR;
    const unsigned int mask = (1u << IO_BIT);
    unsigned int reg_val = *gpi_reg;
    return (reg_val & mask) >> IO_BIT;
}