#include "../inc/sx1262_driver.h"
#include "../inc/hal.h"
#include "../inc/driver.h"

void sx1262_wait_while_busy() {
    while(digital_read(GPI_LORA_BUSY_BIT)) { } // Wait until lora is not busy    
}

/**
 * Tests that SPI is communicating correctly with the radio.
 * If this fails, check your SPI wiring.
 * We test the radio by reading a register that should have a known value.
 * @return True if radio is communicating over SPI. False if no connection.
 */
unsigned int sx1262_sanity_check() {
    unsigned int opcode = 0x1D; // opcode for "read register"
    unsigned int addressToRead = 0x0741; // address of LoRa register
    unsigned int rx = 0;
    unsigned int tx = 0;
    unsigned int expected = 0x24;

    // Example SPI transaction: read a register from the LoRa radio
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    
    // send opcode
    sx1262_wait_while_busy(); // Wait until lora is not busy    

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
 * Set LoRa in standby mode running on the RC oscillator
 * or in sleep mode without the oscillator for lowest power consumption
 * mode = 0 for standby with RC oscillator, mode = 1 for sleep without oscillator
 * See mode #defines in sx1262_driver.h for more details
 * @param mode The power mode to set the LoRa radio in (0 for standby with RC oscillator, 1 for sleep without oscillator)
 */
void sx1262_set_power_mode(unsigned int mode) {
    unsigned int tx = 0;
    unsigned int rx = 0;
    
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command    

    // send opcode for set standby command
    tx = mode ? SET_SLEEP_OPCODE : SET_STANDBY_OPCODE;
    rx = spi_transfer(tx);


    // send standby configuration byte
    tx = 0x00; // 0x00 for standby with RC oscillator and cold start sleep config
    rx = spi_transfer(tx);

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

/*
 * Set the package configuration to LoRa for the SX1262 radio.
 * 
 */
void sx1262_set_packet_to_lora() {
    unsigned int packet_type = 0x01; // value to set packet type to LoRa
    unsigned int tx = SET_PACKET_TYPE_OPCODE; // opcode for set packet type command
    unsigned int rx = 0;

    // send opcode
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    rx = spi_transfer(tx);

    // send packet type byte
    rx = spi_transfer(packet_type);

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

/**
 * Read the current operation packet type from the SX1262 radio.
 * 0x00: GFSK, 0x01: LoRa, 0x03: LR-FHSS
 * @return The current packet type configured in the SX1262 radio (0x00 for
 */
unsigned int sx1262_get_packet() {
    unsigned int rx = 0;
    // send opcode
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    spi_transfer(GET_PACKET_TYPE_OPCODE);
    
    // send two dummy bytes to receive response (first byte is dummy byte, second byte will have the packet type)
    rx = spi_transfer(0x00); 
    rx = spi_transfer(0x00); 
    
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
    
    return rx; // packet type will be in the last byte received
}

/**
 * This function configures the DIO2 pin to internally control RF switch for TX/RX switching.
 * Keep external RF_SW pin high and the radio will automatically switch between TX and RX modes.
 */
void sx1262_setDIO2AsRFSW() {
    unsigned int enable = 0x01; // value to set packet type to LoRa
    unsigned int tx = SET_DIO2_AS_RF_SW_OPCODE; // opcode for set DIO2 as RF switch command
    unsigned int rx = 0;

    // send opcode
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    rx = spi_transfer(tx);

    // send packet type byte
    rx = spi_transfer(enable);

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
    
}

/**
 * Set the operating frequency of the SX1262 radio by writing the appropriate PLL steps to the frequency register.
 * See sx1262_driver.h for defines of PLL steps to set an according frequencies.
 * @param pll_steps The PLL steps value to set the frequency to (see sx1262_driver.h for defines)
 */
void sx1262_set_frequency(unsigned int pll_steps) {
    unsigned int tx = SET_FREQUENCY_OPCODE; // opcode for set frequency command
    unsigned int rx = 0;

    // send opcode
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    rx = spi_transfer(tx);

    for (int i = 24; i >= 0; i -= 8) {
        // send PLL steps byte by byte, starting with the most significant byte
        tx = (pll_steps >> i) & 0xFF;
        rx = spi_transfer(tx);
    }

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave

}

/**
 * Set the LoRa modulation parameters for the SX1262 radio.
 * Set defines for modulation parameters in sx1262_driver.h. 
 * Adjust these defines to change the modulation parameters.
 */
void sx1262_set_modulation_params() {

    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command

    spi_transfer(SET_MODULATION_PARAMS_OPCODE); //send opcode
    spi_transfer(LORA_SF); //ModParam1 = Spreading Factor.  Can be SF5-SF12, written in hex (0x05-0x0C)
    spi_transfer(LORA_BW); //ModParam2 = Bandwidth.  Can be BW500-BW125, written in hex (0x00-0x07)
    spi_transfer(LORA_CR); //ModParam3 = Coding Rate.  Can be CR45-CR48, written in hex (0x01-0x04)
    spi_transfer(LORA_LDRO); //ModParam4 = Low Data Rate Optimization.  Can be 0x00 (disabled) or 0x01 (enabled)

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

/**
 * Set the LoRa modulation parameters for the SX1262 radio.
 * Set defines for modulation parameters in sx1262_driver.h. 
 * Adjust these defines to change the modulation parameters.
 */
void sx1262_set_pa_config() {

    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command

    spi_transfer(SET_PA_CONFIG_OPCODE); //send opcode
    spi_transfer(PA_DC); //ModParam1 = PA duty cycle.  Can be 0x00-0x04, written in hex
    spi_transfer(PA_HPM); //ModParam2 = PA max power.  Can be 0x00-0x07, written in hex
    spi_transfer(DEVICE_SEL); //ModParam3 = Device selection.  Can be 0x00 for SX1262, 0x01 for SX1261
    spi_transfer(PA_LUT); //ModParam4 = PA lookup table.  Always set to 0x01

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

/**
 * Set the LoRa modulation parameters for the SX1262 radio.
 * Set defines for modulation parameters in sx1262_driver.h. 
 * Adjust these defines to change the modulation parameters.
 */
void sx1262_set_tx_params() {

    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command

    spi_transfer(SET_TX_PARAMS_OPCODE); //send opcode
    spi_transfer(TX_POWER); //ModParam1 = TX power.  Can be -17(0xEF) to +14(0x0E) in Low Pow mode.  -9(0xF7) to 22(0x16) in high power mode
    spi_transfer(RAMP_TIME_US); //ModParam2 = Ramp time.  Can be 0x00 (10 us) to 0x07 (3.4 ms), written in hex

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

void sx1262_set_dio_irq_params() {
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    spi_transfer(SET_DIO_IRQ_PARAMS_OPCODE); //send opcode
    
    // byte 1-2 IRQ mask
    spi_transfer((IRQ_MASK >> 8) & 0xFF); // IRQ mask byte 1 (upper byte)
    spi_transfer(IRQ_MASK & 0xFF); // IRQ mask byte 2 (lower byte)

    // byte 3-4 DIO1 mask (map all IRQs enabled by IRQ_MASK to DIO1)
    spi_transfer((DIO1_MASK >> 8) & 0xFF); // DIO1 mask byte 1 (upper byte)
    spi_transfer(DIO1_MASK & 0xFF); // DIO1 mask byte 2 (lower byte)

    // byte 5-8 DIO2 and DIO3 mask are n/a for LoRa and will just be set to 0
    spi_transfer(0x00);
    spi_transfer(0x00);
    
    spi_transfer(0x00);
    spi_transfer(0x00);

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

// transmit 32 bit (an unsigned int).
// @TODO: implement a way to send larger payloads
void sx1262_transmit(unsigned int payload) {
    // Set in standby mode
    sx1262_set_power_mode(STANDBY_RC_MODE);

    // write: "setPacketParams"
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    spi_transfer(SET_PACKET_PARAMS_OPCODE); // send opcode for set packet params command
    spi_transfer((PREAMBLE_LENGTH >> 8) & 0xFF); // send preamble length byte 1 (upper byte)
    spi_transfer(PREAMBLE_LENGTH & 0xFF); // send preamble length byte 2 (lower byte)
    spi_transfer(HEADER_TYPE); // send header type byte
    spi_transfer(0x04); // DEBUG: right now we only send 4 bytes of payload
    spi_transfer(CRC_TYPE); // send CRC type byte
    spi_transfer(INVERT_IQ); // send invert IQ byte
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave

    // Write to FIFO buffer
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    spi_transfer(WRITE_BUFFER_OPCODE); // send opcode for write buffer command
    spi_transfer(0x00); // send buffer offset byte
    
    // this needs to be better
    spi_transfer((payload >> 24) & 0xFF); // send payload byte 1 (most significant byte)
    spi_transfer((payload >> 16) & 0xFF); // send payload byte 2
    spi_transfer((payload >> 8) & 0xFF); // send payload byte 3
    spi_transfer(payload & 0xFF); // send payload byte 4 (least significant byte)

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave


    // Transmit the packet
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    spi_transfer(SET_TX_OPCODE); // send opcode for set TX command
    spi_transfer(0xFF); // send timeout byte 1
    spi_transfer(0xFF); // send timeout byte 2 
    spi_transfer(0xFF); // send timeout byte 3 
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

void sx1262_configure_essentials() {
    // configure DIO2 as RF switch
    sx1262_setDIO2AsRFSW();
    
    // set frequency to 868 MHz
    sx1262_set_frequency(FREQ_868MHz_PLL_STEPS);
    
    // set packet type to LoRa
    sx1262_set_packet_to_lora();
    
    // set Rx timeout to reset on SyncWord or Header detection
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    spi_transfer(STOP_TIMER_ON_HEADER_OPCODE);
    spi_transfer(LOW); // 0x00 to stop timer on SyncWord or Header detection
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave

    // set modulation params (spreading factor, bandwidth, coding rate, low data rate optimization) based on defines in sx1262_driver.h
    sx1262_set_modulation_params();

    // Set PA config (power amplifier)
    sx1262_set_pa_config();

    // Set TX params (power and ramp time)
    sx1262_set_tx_params();

    // Set the number of symbols used by the modem to validate a successful reception
    // Remeber to set the preabmle lenght to be equal or longer than this value for successful receptions
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    spi_transfer(SET_LORA_SYMB_NUM_TIMEOUT_OPCODE); // send opcode
    spi_transfer(0x00); // set symbols (must be even numbers)
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave

    // Enable interrupts and map them to DIO pins based on defines in sx1262_driver.h
    sx1262_set_dio_irq_params();
}
