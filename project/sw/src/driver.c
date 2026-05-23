#include "../inc/hal.h"
#include "../inc/driver.h"

/**
 * Write a 32-bit hexadecimal value to the UART.
 * @param value The value to write
 * @param nibbles The number of nibbles to display
 * @param newline Whether to add a newline character
 */
void print_hex(unsigned int value, unsigned int nibbles, unsigned int newline) {
    for (int i = (nibbles - 1); i >= 0; i--) {
        unsigned int nibble = (value >> (i * 4)) & 0xF;
        if (nibble < 10)
            uart_write_byte('0' + nibble);
        else
            uart_write_byte('A' + (nibble - 10));
    }
    if (newline) {
        uart_write_byte('\r'); // Optional: newline for readability
        uart_write_byte('\n');
    }

}

/**
 * Prints a string to the UART.
 * @param str The null-terminated string to print
 */
void print_str(const char *str)
{
    while (*str)
    {
        uart_write_byte(*str);
        str++;
    }
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
 * Make one SPI transaction.
 * @param tx The data to transmit
 * @return The received data
 */
unsigned int spi_transfer(unsigned int tx)
{
    // Wait until not busy
    while (spi_read_reg(SPI_STATUS_ADDR) & 0x1) {}

    // Write TX data register
    spi_write_reg(SPI_TX_DAT_ADDR, tx);

    // Trigger start transaction pulse
    unsigned int ctrl = spi_read_reg(SPI_CNTRL_ADDR);
    spi_write_reg(SPI_CNTRL_ADDR, ctrl | 0x1);

    // Wait for data_valid
    while (!(spi_read_reg(SPI_STATUS_ADDR) & 0x2)) {}

    // Read RX data for transaction(this clears data_valid)
    return (unsigned int)(spi_read_reg(SPI_RX_DAT_ADDR) & 0xFFFFFFFF);
}


/**
 * Tests that SPI is communicating correctly with the radio.
 * If this fails, check your SPI wiring.  This does not require any setup to run.
 * We test the radio by reading a register that should have a known value.
 * @return True if radio is communicating over SPI. False if no connection.
 */
unsigned int sx1262_sanity_check() {
    unsigned int opcode = 0x1D; // opcode for "read register"
    unsigned int addressToRead = 0x0741; // address of LoRa register
    unsigned int rx = 0;
    unsigned int tx = 0;
    unsigned int expected = 0x24;

    spi_configure(0, 0, 4); // clk_mode=0, data_mode=0, div=4 (0.75 MHz SPI clock for 12 MHz clock. 12MHz / (2^div))

    // Example SPI transaction: read a register from the LoRa radio
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    
    // send opcode
    while(digital_read(GPI_LORA_BUSY_BIT)) { } // Wait until lora is not busy    

    tx = opcode; // send opcode first
    rx = spi_transfer(tx);

    // send address[15:8]
    tx = (addressToRead >> 8) & 0xFF;
    rx = spi_transfer(tx);

    // send address[7:0]
    tx = addressToRead & 0xFF;
    rx = spi_transfer(tx);

    // send dummy byte
    tx = 0x00;
    rx = spi_transfer(tx);

    // send dummy byte to receive data
    tx = 0x00;
    rx = spi_transfer(tx);

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave

    return rx == expected; // return whether the read value matches the expected value

}

unsigned int accel_sanity_check() {
    unsigned int regAddress = 0x0F; // accelerometer "WHO AM I" register address
    unsigned int rx = 0;
    unsigned int tx = 0;
    unsigned int expected = 0x24;

}


