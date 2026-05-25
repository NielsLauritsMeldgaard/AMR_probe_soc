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
 * This function writes a byte to the SPI_TX_DAT register, triggers a transaction, waits for it to complete, and returns the received byte from the SPI_RX_DAT register.
 * It waits for the SPI controller to be not busy before starting, and waits for the data_valid bit to be set before reading the response.
 * It does NOT handle chip select or spi configuration, so those must be done separately before calling this function.
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

/**
 * Read or write to a register on the accelerometer over SPI
 * bit 0: RW bit. 1 for read, 0 for write
 * bit 1: MS bit. 0 for unchanged address, 1 for auto-increment address (for multi-byte reads/writes)
 * bit [7:2]: register address bits [5:0]
 * bit [15:8]: data byte to write (for write operations) or ignored (for read operations)
 * @param regAddress The 6-bit register address to read/write (see ILS328 datasheet for register map)
 * @param value The byte value to write for write operations. Ignored for read operations.
 * @param RW 1 for read operation, 0 for write operation
 * @param MS 1 to auto-increment register address for multi-byte read/write, 0 to keep address unchanged (useful for reading/writing multiple bytes from/to the same register)
 * @return For read operations, returns the byte value read from the register (in the upper byte of the return value). For write operations, returns the raw response from the SPI transfer (which may be ignored).
 */
unsigned int accel_rw_register(unsigned int regAddress, unsigned int value, unsigned int RW, unsigned int MS)
{
    unsigned int tx = 0;
    unsigned int rx = 0;

    spi_configure(0, 0, 4); // clk_mode=0, data_mode=0, div=4 (0.75 MHz SPI clock for 12 MHz clock. 12MHz / (2^div))

    digital_write(LOW, GPO_ACCEL_CS_BIT); // set accel CS low to select the slave
    
    // while(digital_read(GPI_ACCEL_INT1_BIT)) { } // Wait until accel is not busy    

    // Send command byte with RW, MS, and register address
    tx = ((regAddress & 0x3F) << 0) | ((MS & 0x1) << 6) | ((RW & 0x1) << 7);
    rx = spi_transfer(tx);

    if (RW) { // if read operation, send dummy byte to receive data
        tx = 0x00;
        rx = spi_transfer(tx);
    } else { // if write operation, send data byte
        tx = (value & 0xFF); // data byte goes in upper byte of the command
        rx = spi_transfer(tx);
    }

    digital_write(HIGH, GPO_ACCEL_CS_BIT); // set accel CS high to deselect the slave

    return rx; // for read operations, the register value will be in the upper byte of the response
    

}


unsigned int accel_sanity_check() {
    // WHO AM I default value should be 0x32 for ILS328
    unsigned int who_am_i_reg = 0x0F; // WHO_AM_I register address for ILS328 accelerometer
    unsigned int expected = 0x32;
    unsigned int actual = accel_rw_register(who_am_i_reg, 0, 1, 0); // read WHO_AM_I register
    //debug 
    // print_hex(actual, 2, 1); // print the read WHO_AM_I value for debugging
    return actual == expected;
}


