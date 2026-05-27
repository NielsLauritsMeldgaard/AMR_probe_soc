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
 * Write an unsigned integer as a decimal string to the UART.
 * @param v The value to write (will be capped at 65535)
 * @param newline Whether to add a newline character
 */
void print_dec_u16(unsigned int v, unsigned int newline) {
    if (v > 65535u) v = 65535u;

    unsigned int d;
    int started = 0;

    d = 0; while (v >= 10000u) { v -= 10000u; d++; }
    if (d || started) { uart_write_byte('0' + d); started = 1; }

    d = 0; while (v >= 1000u) { v -= 1000u; d++; }
    if (d || started) { uart_write_byte('0' + d); started = 1; }

    d = 0; while (v >= 100u) { v -= 100u; d++; }
    if (d || started) { uart_write_byte('0' + d); started = 1; }

    d = 0; while (v >= 10u) { v -= 10u; d++; }
    if (d || started) { uart_write_byte('0' + d); started = 1; }

    uart_write_byte('0' + (unsigned int)v);

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
    return actual == expected;
}

/**
 * Read all 24-bit packets for the four channels on the ADC2 (coarse suntracker ADC) over SPI
 * ADC is the 16-bit SAR MUX LTC2357-16 from Analog Devices.
 * Remeber to configure the spi controller before calling this function and to set the correct softcode for the ADC2 (see SOFTCODE define in driver.h and datasheet for details on how to generate the softcode).
 * @param ch0 Pointer to store the 16-bit value read from ADC2 channel 0
 * @param ch1 Pointer to store the 16-bit value read from ADC2 channel 1
 * @param ch2 Pointer to store the 16-bit value read from ADC2 channel 2
 * @param ch3 Pointer to store the 16-bit value read from ADC2 channel 3
 * Note: This function will read two samples from the ADC2. The first sample is used to send the softcode and start the conversion, and the second sample is the actual converted values from the four channels.
 */
void read_ADC2(unsigned int *ch0, unsigned int *ch1, unsigned int *ch2, unsigned int *ch3) {
    unsigned int adc2_rx_hi = 0;
    unsigned int adc2_rx_mi = 0;
    unsigned int adc2_rx_lo = 0;

    digital_write(LOW, GPO_ADC2_CS_BIT);        // set ADC2 CS low to select the slave
    digital_write(LOW, GPO_ADC2_SHDN_BIT);      // wake up ADC2
    while( digital_read(GPI_ADC2_BUSY_BIT) );   // wait while ADC2 starting up

    // read 2 samples from ADC2. First it needs to be programmed with the softcode and then it can be read. 
    for (unsigned int n = 0; n < 2; n++) {
        digital_write(HIGH, GPO_ADC2_CNV_BIT);      // pulse convert bit to start conversion
        digital_write(LOW, GPO_ADC2_CNV_BIT);
        while( digital_read(GPI_ADC2_BUSY_BIT) );   // wait while ADC2 is busy
        
        // read all 4 channels from ADC2
        for (unsigned int i = 0; i < 4; i++) {
            adc2_rx_hi = spi_transfer(SOFTCODE >> 8);   // send upper byte of softcode
            adc2_rx_mi = spi_transfer(SOFTCODE & 0xFF); // send lower byte of softcode
            adc2_rx_lo = spi_transfer(0x00);            // dummy transfer channel id and used softcode. Currentlty ignored but can be used for debugging or to verify that the correct softcode is being used.
            
            if (n > 0) {
                if (i == 0) {
                    *ch0 = ((adc2_rx_hi << 8) | adc2_rx_mi) & 0xFFFF; // 16-bit ADC value is in the upper byte and lower 4 bits of the middle byte
                } else if (i == 1) {
                    *ch1 = ((adc2_rx_hi << 8) | adc2_rx_mi) & 0xFFFF;
                } else if (i == 2) {
                    *ch2 = ((adc2_rx_hi << 8) | adc2_rx_mi) & 0xFFFF;
                } else if (i == 3) {
                    *ch3 = ((adc2_rx_hi << 8) | adc2_rx_mi) & 0xFFFF;
                }
            }
        }
    }

    digital_write(HIGH, GPO_ADC2_CS_BIT);   // deselect the slave
    digital_write(HIGH, GPO_ADC2_SHDN_BIT); // set ADC2 back to shutdown
}


