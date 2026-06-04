#include "../inc/sx1262_driver.h"
#include "../inc/hal.h"
#include "../inc/driver.h"

/**
 * Helper function to wait while the SX1262 radio is busy.
 * It simply check on the physical BUSY pin and hangs until the radio is no longer busy.
 */
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

    sx1262_wait_while_busy(); // wait until radio is not busy before sending command    
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave

    // send opcode for set standby command
    tx = mode ? SET_SLEEP_OPCODE : SET_STANDBY_OPCODE;
    spi_transfer(tx);

    // send standby configuration byte
    spi_transfer(0x00); // 0x00 for standby with RC oscillator and cold start sleep config

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

/*
 * Set the package configuration to LoRa for the SX1262 radio.
 */
void sx1262_set_packet_to_lora() {
    // send opcode
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(SET_PACKET_TYPE_OPCODE);
    // send packet type byte
    spi_transfer(HIGH); // value to set packet type to LoRa (0x00 for GFSK, 0x01 for LoRa, 0x03 for LR-FHSS)
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
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
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
void sx1262_set_DIO2_as_RFSW() {
    // send opcode
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(SET_DIO2_AS_RF_SW_OPCODE); // opcode for set DIO2 as RF switch command
    // send packet type byte
    spi_transfer(HIGH); // value to set DIO2 as RF switch (0x00 to disable, 0x01 to enable)
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

/**
 * Set the operating frequency of the SX1262 radio by writing the appropriate PLL steps to the frequency register.
 * See sx1262_driver.h for defines of PLL steps to set an according frequencies.
 * @param pll_steps The PLL steps value to set the frequency to (see sx1262_driver.h for defines)
 */
void sx1262_set_frequency(unsigned int pll_steps) {
    // send opcode
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(SET_FREQUENCY_OPCODE);
    spi_transfer((pll_steps >> 24) & 0xFF);  // MSB of pll frequency
    spi_transfer((pll_steps >> 16) & 0xFF);  // 
    spi_transfer((pll_steps >> 8)  & 0xFF);  // 
    spi_transfer((pll_steps >> 0)  & 0xFF);  // LSB of pll frequency

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

/**
 * Set the LoRa modulation parameters for the SX1262 radio.
 * Set defines for modulation parameters in sx1262_driver.h. 
 * Adjust these defines to change the modulation parameters.
 */
void sx1262_set_modulation_params() {

    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave

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

    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave

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

    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave

    spi_transfer(SET_TX_PARAMS_OPCODE); //send opcode
    spi_transfer(TX_POWER); //ModParam1 = TX power.  Can be -17(0xEF) to +14(0x0E) in Low Pow mode.  -9(0xF7) to 22(0x16) in high power mode
    spi_transfer(RAMP_TIME_US); //ModParam2 = Ramp time.  Can be 0x00 (10 us) to 0x07 (3.4 ms), written in hex

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

/**
 * Configure DIO IRQ params to set which events trigger an interrupt on which DIO pin.
 */
void sx1262_set_dio_irq_params() {
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
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

/**
 * Get the current status of the SX1262 radio, including the chip mode and command status.
 * Chip mode indicates the current operating mode of the radio (e.g. standby, sleep, transmit, receive, etc.)
 * Command status indicates the status of the last command sent to the radio (e.g. if there was an error with the command, or if the command is still being processed, etc.)
 * @param chip_mode Pointer to an unsigned int variable where the chip mode will be stored
 * @param command_status Pointer to an unsigned int variable where the command status will be stored
 */
void sx1262_get_status(unsigned int *chip_mode, unsigned int *command_status) {
    unsigned int rx = 0;
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(GET_STATUS_OPCODE); // send opcode for get status command
    rx = spi_transfer(0x00); // send dummy byte to receive chip mode in response
    *chip_mode = (rx >> 4) & 0x07; // //Chip mode is bits [6:4] (3-bits)
    *command_status = (rx >> 1) & 0x07; // Command status is bits [3:1] (3-bits)
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

/**
 * Get the device errors from the SX1262 radio.
 * @param status Pointer to an unsigned int variable where the status will be stored
 * @param op_error Pointer to an unsigned int variable where the operation error will be stored
 */
void sx1262_get_device_error(unsigned int *status, unsigned int *op_error) {
    unsigned int op_error_lb = 0;
    unsigned int op_error_ub = 0;
    sx1262_wait_while_busy();
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(GET_DEVICE_ERRORS_OPCODE); // send opcode for get device errors command
    *status = spi_transfer(0x00); // send NOP to get staus
    op_error_ub = spi_transfer(0x00); // upper byte of operation errors
    op_error_lb = spi_transfer(0x00); // lower byte of operation errors
    *op_error = (op_error_ub << 8) | op_error_lb; // combine upper and lower byte to get full operation error value
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high

}

/**
 * Clear the device errors in the SX1262 radio by sending the clear device errors command.
 */
void sx1262_clear_device_errors() {
    sx1262_wait_while_busy();
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(CLEAR_DEVICE_ERRORS_OPCODE); // send opcode for clear device errors command
    spi_transfer(0x00); // send dummy byte
    spi_transfer(0x00); // send dummy byte
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}


/**
 * Configure the DIO3 pin to output a voltage to power an external TCXO and set the necessary delay for the TCXO to stabilize.
 */
void sx1262_set_dio3_as_tcxo() {
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(SET_DIO3_AS_TCXO_CTRL_OPCODE); // send opcode for set DIO3 as TCXO command
    spi_transfer(TCXO_VOLTAGE_SETTING); // send TCXO voltage setting byte
    spi_transfer((TCXO_SETTLE_TIME_STEPS >> 16) & 0xFF); // send upper byte of settle time
    spi_transfer((TCXO_SETTLE_TIME_STEPS >> 8) & 0xFF); // send middle byte of settle time
    spi_transfer(TCXO_SETTLE_TIME_STEPS & 0xFF); // send lower byte of settle time
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

void sx1262_set_packet_params() {
    // write: "setPacketParams"
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(SET_PACKET_PARAMS_OPCODE);         // send opcode for set packet params command
    spi_transfer((PREAMBLE_LENGTH >> 8) & 0xFF);    //PacketParam1 = Preamble Len MSB
    spi_transfer(PREAMBLE_LENGTH & 0xFF);           //PacketParam2 = Preamble Len LSB
    spi_transfer(HEADER_TYPE);                      //PacketParam3 = Header Type. 0x00 = Variable Len, 0x01 = Fixed Length
    spi_transfer(PAYLOAD_LENGTH);                   //PacketParam4 = Payload Length (Max is 255 bytes).
    spi_transfer(CRC_TYPE);                         //PacketParam5 = CRC Type. 0x00 = Off, 0x01 = on
    spi_transfer(INVERT_IQ);                        //PacketParam6 = Invert IQ.  0x00 = Standard, 0x01 = Inverted
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
}

unsigned int sx1262_wait_command_completion() {
    unsigned int spi_data_transmitted = LOW;
    unsigned int chip_mode = 0;
    unsigned int command_status = 0;

    while (!spi_data_transmitted) {
        delay_cycles(100); // don't spam SPI 
        sx1262_get_status(&chip_mode, &command_status);


        //Status 0, 1, 2 mean we're still busy.  Anything else means we're done.
        //Commands 3-6 = command timeout, command processing error, failure to execute command, and Tx Done (respectively)
        if (command_status != 0 && command_status != 1 && command_status != 2) {
            spi_data_transmitted = HIGH; // set flag to indicate command is done processing
        }

        //If we're in standby mode, we don't need to wait at all
        //0x03 = STBY_XOSC, 0x02= STBY_RC
        if (chip_mode == 0x03 || chip_mode == 0x02) {
            spi_data_transmitted = HIGH;
        }

        // print_str("[LOG] - SX1262: Chip mode: 0x");
        // print_hex(chip_mode, 2, 0);
        // print_str(", Command status: 0x");
        // print_hex(command_status, 2, 0);
        // sx1262_get_device_error(&sx1262_status, &sx1262_errors);
        // print_str(", Device errors: 0x");
        // // print_hex(sx1262_status, 2, 0);
        // print_hex(sx1262_errors, 4, 1);  
    }
    return spi_data_transmitted;
}

void sx1262_set_mode_rx() {
    // Set in standby mode first
    sx1262_set_power_mode(STANDBY_RC_MODE);

    // write packet params
    sx1262_set_packet_params();

    // Set radio in rx mode with no timeout (continuous reception)
    // Based on IRQ configuration in "sx1262_configure_essentials", the radio will generate an interrupt on DIO1 when a packet is received and CRC is ok
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(SET_RX_OPCODE); // send opcode for set RX command
    spi_transfer(0xFF); // 24-bit timeout, 0xFFFFFF means no timeout and continuous reception
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave 

    sx1262_wait_while_busy(); // wait until the radio is in RX mode before exiting the function
    //sx1262_wait_command_completion(); // wait until command is done processing (either Rx Done or error)
}

// @TODO: Handle payloads greater than 4 bytes
unsigned int sx1262_receive_async(unsigned int *payload) {

    unsigned int chip_mode = 0;
    unsigned int command_status = 0;
    sx1262_get_status(&chip_mode, &command_status); // get current chip mode and command status
    if (chip_mode != 0x05) { // if we're not already in rx mode, set the mode to rx
        sx1262_set_mode_rx();
    }

    // Read pin status of DIO1 to check for received packet
    // Based on the IRQ configuration in "sx1262_configure_essentials", DIO1 will go high when a packet is received and CRC is ok
    if (digital_read(GPI_LORA_DIO1_BIT)) {
        // clear all interrupt flags
        while (digital_read(GPI_LORA_DIO1_BIT)) { // while DIO1 is still high (should only be one packet received, so should only loop once)
            sx1262_wait_while_busy(); // wait until radio is not busy before sending command
            digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
            spi_transfer(0x02); //Opcode for ClearIRQStatus command
            spi_transfer(0xFF); //IRQ bits to clear (MSB) (0xFFFF means clear all interrupts)
            spi_transfer(0xFF); //IRQ bits to clear (LSB)
            digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave
        }
    } else {
        return LOW; // no packet received
    }

    // read rx buffer status to get the number of bytes received and the offset in the buffer where the received packet is stored
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low
    spi_transfer(0x13); // opcode for GetRxBufferStatus command
    spi_transfer(0x00); // NOP. returns radio status
    unsigned int payload_len = spi_transfer(0x00); // returns number of bytes received
    unsigned int start_addr = spi_transfer(0x00); // returns offset in buffer where received packet is stored
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high

    // read received packet from buffer
    // DEBUG: payload size may not exceed 4 bytes
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low
    spi_transfer(0x1E); // Opcode for ReadBuffer command
    spi_transfer(start_addr); // SX1262 memory location to start reading from
    spi_transfer(0x00); // send dummy byte

    // assemble payload
    unsigned int payload_ = 0;
    payload_ |= (spi_transfer(0x00) << 24); // read byte 1 (MSB)
    payload_ |= (spi_transfer(0x00) << 16); // read byte 2
    payload_ |= (spi_transfer(0x00) << 8); // read byte 3
    payload_ |= spi_transfer(0x00); // read byte 4 (LSB)

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high

    *payload = payload_; // store received payload in variable provided as argument
    return payload_len;
}


// transmit 32 bit (an unsigned int).
// @TODO: implement a way to send larger payloads
void sx1262_transmit(unsigned int payload) {
    // Set in standby mode
    sx1262_set_power_mode(STANDBY_RC_MODE);

    // write: "setPacketParams"
    sx1262_set_packet_params();

    // Write to FIFO buffer
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(WRITE_BUFFER_OPCODE); // send opcode for write buffer command
    spi_transfer(0x00); // send buffer offset byte
    
    // this needs to be better
    spi_transfer((payload >> 24) & 0xFF); // send payload byte 1 (most significant byte)
    spi_transfer((payload >> 16) & 0xFF); // send payload byte 2
    spi_transfer((payload >> 8) & 0xFF); // send payload byte 3
    spi_transfer(payload & 0xFF); // send payload byte 4 (least significant byte)

    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave


    // Transmit the packet
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(SET_TX_OPCODE); // send opcode for set TX command
    spi_transfer(0xFF); // send timeout byte 1
    spi_transfer(0xFF); // send timeout byte 2 
    spi_transfer(0xFF); // send timeout byte 3 
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave

    sx1262_wait_command_completion(); // wait until command is done processing (either Tx Done or error)
}

/**
 * Set the private sync word for the SX1262 radio.
 */
void sx1262_set_sync_word_private() {
    sx1262_wait_while_busy();
    digital_write(LOW, GPO_LORA_CS_BIT);
    spi_transfer(0x0D);      // Opcode: Write Register
    spi_transfer(0x07);      // Address MSB
    spi_transfer(0x40);      // Address LSB (0x0740)
    spi_transfer(0x14);      // Sync Word MSB
    spi_transfer(0x24);      // Sync Word LSB (Private)
    digital_write(HIGH, GPO_LORA_CS_BIT);
}

/**
 * Set the regulator mode for the SX1262 radio.
 */
void sx1262_set_regulator_mode() {
    sx1262_wait_while_busy();
    digital_write(LOW, GPO_LORA_CS_BIT);
    spi_transfer(0x96); // SetRegulatorMode Opcode
    spi_transfer(0x01); // 0x01 = Enable DC-DC + LDO
    digital_write(HIGH, GPO_LORA_CS_BIT);
}

/**
 * This function configures the essential settings for the SX1262 radio to operate in LoRa mode.
 * It sets the regulator mode, configures the hardware pins, sets the modulation parameters, and configures the interrupts and calibration.
 * This function should be called before attempting to transmit or receive data with the SX1262 radio.
 */
void sx1262_configure_essentials() {
    // clear erros
    sx1262_clear_device_errors();

    // Set regulator to DC-DC (RadioLib default is DC-DC enabled)
    sx1262_set_regulator_mode();

    // Configure hardware pins
    sx1262_set_DIO2_as_RFSW();
    sx1262_set_dio3_as_tcxo();
    
    // Radio settings
    sx1262_set_packet_to_lora();
    // sx1262_set_frequency(FREQ_434MHz_PLL_STEPS);
    sx1262_set_frequency(FREQ_868MHz_PLL_STEPS);
    sx1262_set_modulation_params();
    sx1262_set_pa_config();
    sx1262_set_tx_params();

    // Set private word sync
    sx1262_set_sync_word_private();
    
    
    // set Rx timeout to reset on SyncWord or Header detection
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(STOP_TIMER_ON_HEADER_OPCODE);
    spi_transfer(LOW); // 0x00 to stop timer on SyncWord or Header detection
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave

    // interrupts and calibration
    sx1262_set_dio_irq_params();

    // // Image Calibration for 434 band (Freq1: 0x6B, Freq2: 0x6F)
    // sx1262_wait_while_busy();
    // digital_write(LOW, GPO_LORA_CS_BIT);
    // spi_transfer(0x98); 
    // spi_transfer(0x6B); 
    // spi_transfer(0x6F); 
    // digital_write(HIGH, GPO_LORA_CS_BIT);
    // sx1262_wait_while_busy();

    // Image Calibration for the 868MHz Band
    // Datasheet Table 9-2: 863-870 MHz range uses 0xD7 and 0xDB
    sx1262_wait_while_busy();
    digital_write(LOW, GPO_LORA_CS_BIT);
    spi_transfer(0x98); 
    spi_transfer(0xD7); // Freq1
    spi_transfer(0xDB); // Freq2
    digital_write(HIGH, GPO_LORA_CS_BIT);
    sx1262_wait_while_busy();


    // Set the number of symbols used by the modem to validate a successful reception
    // Remeber to set the preabmle lenght to be equal or longer than this value for successful receptions
    sx1262_wait_while_busy(); // wait until radio is not busy before sending command
    digital_write(LOW, GPO_LORA_CS_BIT); // set LoRa CS low to select the slave
    spi_transfer(SET_LORA_SYMB_NUM_TIMEOUT_OPCODE); // send opcode
    spi_transfer(0x00); // set symbols (must be even numbers)
    digital_write(HIGH, GPO_LORA_CS_BIT); // set LoRa CS high to deselect the slave

}