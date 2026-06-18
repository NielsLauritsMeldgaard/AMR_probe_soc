#include "../inc/hal.h"
#include "../inc/driver.h"
#include "../inc/sx1262_driver.h"

// Opcode Definitions
#define OP_BATTERY 0x01
#define OP_HMC     0x02
#define OP_ACCEL   0x03
#define OP_PD      0x04

// Helper: Sends a standardized 4-byte response to the Master
void send_response(unsigned int opcode, unsigned int sub_id, unsigned int data) {
    unsigned int tx_payload = ((unsigned int)opcode << 24) | 
                              ((unsigned int)sub_id << 16) | 
                              (data & 0xFFFF);
    
    sx1262_transmit(tx_payload);
    
    unsigned int mode, status;
    sx1262_get_status(&mode, &status);
    
    if (status != 0x06) {
        print_str("[LOG] TX ERROR: 0x");
        print_hex(status, 2, 1);
    }
}

// Helper: Prints current sensor state in CSV format for PC logging
void print_csv(unsigned int v, int h1, int h2, int h3, int a1, int a2, int a3, int p1, int p2, int p3, int p4) {
    // Format: BAT, HMC1, HMC2, HMC3, ACCEL1, ACCEL2, ACCEL3, PD1, PD2, PD3, PD4
    print_dec_u16(v, 0);  print_str(",");
    print_dec_u16(h1, 0); print_str(",");
    print_dec_u16(h2, 0); print_str(",");
    print_dec_u16(h3, 0); print_str(",");
    print_dec_u16(a1, 0); print_str(",");
    print_dec_u16(a2, 0); print_str(",");
    print_dec_u16(a3, 0); print_str(",");
    print_dec_u16(p1, 0); print_str(",");
    print_dec_u16(p2, 0); print_str(",");
    print_dec_u16(p3, 0); print_str(",");
    print_dec_u16(p4, 1); // Ends with \r\n
}

int main(void) {
    // --- Hardware Initialization ---
    set_counter();
    // set SPI slaves to inactive (CS high)
    digital_write(HIGH, GPO_ACCEL_CS_BIT);
    digital_write(HIGH, GPO_LORA_CS_BIT);
    digital_write(HIGH, GPO_ADC2_CS_BIT);
    digital_write(HIGH, GPO_LORA_RF_SW_BIT);    // Set HIGH and let DIO2 control RF switch for TX/RX
    digital_write(LOW, GPO_ADC2_SHDN_BIT);      // set ADC2 to shutdown
    digital_write(LOW, GPO_ADC2_CNV_BIT);       // set ADC2 convert bit to 0
    //digital_write(HIGH, GPO_DAC_SET_BIT);       // set DAC output
    set_leds(0);

    // configure the SPI controller
    spi_configure(0, 0, 4); // clk_mode=0, data_mode=0, div=4 (0.75 MHz SPI clock for 12 MHz clock. 12MHz / (2^div))

    // --- Radio Boot ---
    digital_write(LOW, GPO_LORA_RESET_BIT);
    delay_cycles(10000);
    digital_write(HIGH, GPO_LORA_RESET_BIT);
    sx1262_wait_while_busy();
    sx1262_configure_essentials();
    
    if (!sx1262_sanity_check()) {
        while (1)
        {
            print_str("[LOG] FATAL: SX1262 not found!\r\n");
        }        
    }

    print_str("[LOG] SX1262 online. System Ready.\r\n");

    if (!accel_sanity_check()) {
        while (1)
        {            
            print_str("[LOG] FATAL: Accelerometer not found!\r\n");
        }
    }
    // Register 0x20 (CTRL_REG1)
    accel_rw_register(CTRL_REG1_ADDR, CTRL_REG1_VALUE, LOW, LOW); 
    print_str("[LOG] Accelerometer online. System Ready.\r\n");


    print_str("[LOG] CSV Format: BAT,H1,H2,H3,A1,A2,A3,P1,P2,P3,P4\r\n");

    // Local state variables
    unsigned int vbat, pd1, pd2, p3, pd4;
    int h1, h2, h3, ax, ay, az;
    unsigned int lora_rx_payload, lora_rx_len;
    unsigned int last_error_state = 0;
    unsigned int led1 = 0;
    unsigned int btn_state = 0;
    unsigned int btn_state_prev = 0;
    unsigned int dac_off = 0;

    while (1) {
        btn_state = read_botton();
        if (btn_state == 1 && btn_state_prev == 0) {
            dac_off = !dac_off; // Toggle DAC state
            if (!dac_off) {
                set_leds(3); // Set both LEDs on to indicate DAC is active (example)
            } else {
                set_leds(0); // Set both LEDs off to indicate DAC is off
            }
        }
        btn_state_prev = btn_state;

        // Update Sensors (Fresh data for every loop)
        vbat = read_xadc();
        h1 = read_hmc_axis(1); h2 = read_hmc_axis(2); h3 = read_hmc_axis(3);
        read_ADC2(&pd1, &pd2, &p3, &pd4);
        ax = read_accel_axis(1); ay = read_accel_axis(2); az = read_accel_axis(3);

        // Handle LoRa Inbound (Async)
        lora_rx_len = sx1262_receive_async(&lora_rx_payload);
        if (lora_rx_len > 0) {
            unsigned int opcode = (lora_rx_payload >> 24) & 0xFF;
            unsigned int request_id = (lora_rx_payload >> 16) & 0xFF; // Axis/Channel

            switch(opcode) {
                case OP_BATTERY:
                    send_response(OP_BATTERY, 0, vbat);
                    break;
                
                case OP_HMC:
                    if (request_id == 1)      send_response(OP_HMC, 1, h1);
                    else if (request_id == 2) send_response(OP_HMC, 2, h2);
                    else                      send_response(OP_HMC, 3, h3);
                    break;

                case OP_ACCEL:
                    if (request_id == 1)      send_response(OP_ACCEL, 1, ax);
                    else if (request_id == 2) send_response(OP_ACCEL, 2, ay);
                    else                      send_response(OP_ACCEL, 3, az);
                    break;

                case OP_PD:
                    if (request_id == 1)      send_response(OP_PD, 1, pd1);
                    else if (request_id == 2) send_response(OP_PD, 2, pd2);
                    else if (request_id == 3) send_response(OP_PD, 3, p3);
                    else                      send_response(OP_PD, 4, pd4);
                    break;

                default:
                    print_str("[LOG] Unknown Opcode: 0x");
                    print_hex(opcode, 2, 1);
            }
        }

        // Periodic Logging
        if (read_timer() > 100000) {
            set_counter();
            
            // DEBUG
            //led1 = ~led1 & 0x01;
            //set_leds(led1); // Heartbeat led1 toggle every run
            
            // Output fresh CSV row for PC processing
            print_csv(vbat, h1, h2, h3, ax, ay, az, pd1, pd2, p3, pd4);

            // Report Radio Errors only if they occur
            unsigned int status, op_err;
            sx1262_get_device_error(&status, &op_err);
            if (op_err != 0 && op_err != last_error_state) {
                print_str("[LOG] RADIO WARNING: Error Code 0x");
                print_hex(op_err, 4, 1);
                sx1262_clear_device_errors();
            }
            last_error_state = op_err;
        }
    }
}


// #include "../inc/hal.h"
// #include "../inc/driver.h"
// #include "../inc/sx1262_driver.h"

// #define BATTERY_READ_OPCODE 0x01
// #define HMC_READ_OPCODE 0x02
// #define ACCEL_READ_OPCODE 0x03
// #define PD_READ_OPCODE 0x04

// #define RADIO_RX_DATA_AVAILABLE 0x02
// #define RADIO_TX_DONE_STATUS 0x06

// int main(void)
// {
//     set_counter();

//     // set SPI slaves to inactive (CS high)
//     digital_write(HIGH, GPO_ACCEL_CS_BIT);
//     digital_write(HIGH, GPO_LORA_CS_BIT);
//     digital_write(HIGH, GPO_ADC2_CS_BIT);
//     digital_write(HIGH, GPO_LORA_RF_SW_BIT);    // Set HIGH and let DIO2 control RF switch for TX/RX
//     digital_write(HIGH, GPO_ADC2_SHDN_BIT);     // set ADC2 to shutdown
//     digital_write(LOW, GPO_ADC2_CNV_BIT);       // set ADC2 convert bit to 0
    
//     // configure the SPI controller
//     spi_configure(0, 0, 4); // clk_mode=0, data_mode=0, div=4 (0.75 MHz SPI clock for 12 MHz clock. 12MHz / (2^div))

//     // initialize variables
//     unsigned int timer = 0; 
//     unsigned int led = 0; 
//     unsigned int counter = 0; 

//     // Sensor variables
//     unsigned int vbat = 0;
//     unsigned int HMC_axis_1 = 0;
//     unsigned int HMC_axis_2 = 0;
//     unsigned int HMC_axis_3 = 0;
//     unsigned int PD1 = 0;
//     unsigned int PD2 = 0;
//     unsigned int PD3 = 0;
//     unsigned int PD4 = 0;
//     unsigned int accel_x = 0;
//     unsigned int accel_y = 0;
//     unsigned int accel_z = 0;

//     // SX1262 variables
//     unsigned int sx1262_chip_mode = 0;
//     unsigned int sx1262_command_status = 0;
//     unsigned int sx1262_errors = 0;
//     unsigned int sx1262_status = 0;
//     unsigned int sx1262_payload = 0;
//     unsigned int sx1262_payload_len = 0;
//     unsigned int sx1262_opcode = 0;
//     unsigned int sx1262_param1 = 0;
//     unsigned int sx1262_param2 = 0;
//     unsigned int sx1262_param3 = 0;
//     unsigned int sx1262_tx_payload = 0;
    
//     // initialize lora radio
//     digital_write(LOW, GPO_LORA_RESET_BIT); // assert reset
//     delay_cycles(10000); // delay for reset duration
//     digital_write(HIGH, GPO_LORA_RESET_BIT); // deassert reset
//     sx1262_wait_while_busy(); // wait until radio is done resetting
//     sx1262_configure_essentials();
//     sx1262_set_power_mode(STANDBY_RC_MODE); // set to standby mode with RC oscillator to start
//     if (sx1262_sanity_check()) {
//         print_str("[LOG] - SX1262: boot successful\r\n");        
//     } else {
//         print_str("[LOG] - SX1262: boot failure\r\n");
//     }

//     while (1) {
//         // Read battery voltage from ADC1 and store in vbat variable
//         vbat = read_ADC1_vbat();

//         // read HMC axis values and store in variables
//         HMC_axis_1 = read_hmc_axis(1);
//         HMC_axis_2 = read_hmc_axis(2);
//         HMC_axis_3 = read_hmc_axis(3);

//         // Read ADC2 channels for coarse suntracker and store in variables
//         read_ADC2(&PD1, &PD2, &PD3, &PD4);

//         // Read accelerometer axis values and store in variables
//         accel_x = read_accel_axis(1);
//         accel_y = read_accel_axis(2);
//         accel_z = read_accel_axis(3);

//         // Fast loop: Sensor and radio 
//         if (read_timer() > 1000) {            
//             // Radio task
//             sx1262_payload_len = sx1262_receive_async(&sx1262_payload); // check for received LoRa packets and read payload if available

//             if (sx1262_payload_len) {
//                 sx1262_get_status(&sx1262_chip_mode, &sx1262_command_status);
//                 if (sx1262_command_status == RADIO_RX_DATA_AVAILABLE) {

//                     // parse received payload into opcode and parameters
//                     sx1262_opcode = (sx1262_payload >> 24) & 0xFF;
//                     sx1262_param3 = (sx1262_payload >> 16) & 0xFF;
//                     sx1262_param2 = (sx1262_payload >> 8) & 0xFF;
//                     sx1262_param1 = sx1262_payload & 0xFF;

//                     if (sx1262_opcode == BATTERY_READ_OPCODE) { // if opcode is 0x01, treat as battery read request
                        
//                         print_str("[LOG] - SX1262: Received battery read request\r\n");
                        
//                         // Byte 0 = 0x01, Byte 1 = 0x00, Byte 2 = VBat High, Byte 3 = VBat Low                        
//                         sx1262_tx_payload = (BATTERY_READ_OPCODE << 24) | (vbat & 0xFFFF); // construct battery read response payload with opcode 0x01 and battery voltage in lower 16 bits
//                         sx1262_transmit(sx1262_tx_payload); // transmit battery read response
                        
//                         sx1262_get_status(&sx1262_chip_mode, &sx1262_command_status); // get status after transmission
//                         if (sx1262_command_status == RADIO_TX_DONE_STATUS) { // if command status is 0x06, the packet was transmitted and the radio is now in standby mode
//                             print_str("[LOG] - SX1262: Battery read response transmitted\r\n");
//                         } else {
//                             print_str("[LOG] - SX1262: Error transmitting battery read response, command status: 0x");
//                             print_hex(sx1262_command_status, 2, 1);
//                         }                
//                     }

//                     else if (sx1262_opcode == HMC_READ_OPCODE) { // if opcode is 0x02, treat as HMC read request
//                         print_str("[LOG] - SX1262: Received HMC read request for axis ");
//                         print_dec_u16(sx1262_param3, 1); // parameter 3 contains the axis number to read
                        
//                         if (sx1262_param3 == 1) {
//                             sx1262_tx_payload = HMC_axis_1;
//                         }
//                         else if (sx1262_param3 == 2) {
//                             sx1262_tx_payload = HMC_axis_2;
//                         }
//                         else if (sx1262_param3 == 3) {
//                             sx1262_tx_payload = HMC_axis_3;
//                         }
//                         else {
//                             print_str("[LOG] - SX1262: Invalid HMC axis number in read request: ");
//                             print_dec_u16(sx1262_param3, 1);
//                             sx1262_tx_payload = 0; // return 0 for invalid axis numbers
//                         }

//                         sx1262_tx_payload = (HMC_READ_OPCODE << 24) | (sx1262_param3 << 16) | (sx1262_tx_payload & 0xFFFF); // construct response payload with opcode 0x02, axis number in parameter 3, and axis value in lower 16 bits
//                         sx1262_transmit(sx1262_tx_payload); // transmit HMC read response
                        
//                         sx1262_get_status(&sx1262_chip_mode, &sx1262_command_status); // get status after transmission
//                         if (sx1262_command_status == RADIO_TX_DONE_STATUS) { // if command status is 0x06, the packet was transmitted and the radio is now in standby mode
//                             print_str("[LOG] - SX1262: HMC read response transmitted\r\n");
//                         } else {
//                             print_str("[LOG] - SX1262: Error transmitting HMC read response, command status: 0x");
//                             print_hex(sx1262_command_status, 2, 1);
//                         } 
//                     }
                    
//                     else if (sx1262_opcode == ACCEL_READ_OPCODE) { // if opcode is 0x03, treat as accelerometer read request
//                         print_str("[LOG] - SX1262: Received accelerometer read request for axis ");
//                         print_dec_u16(sx1262_param3, 1); // parameter 3 contains the axis number to read
                        
//                         if (sx1262_param3 == 1) {
//                             sx1262_tx_payload = accel_x;
//                         }
//                         else if (sx1262_param3 == 2) {
//                             sx1262_tx_payload = accel_y;
//                         }
//                         else if (sx1262_param3 == 3) {
//                             sx1262_tx_payload = accel_z;
//                         }
//                         else {
//                             print_str("[LOG] - SX1262: Invalid accelerometer axis number in read request: ");
//                             print_dec_u16(sx1262_param3, 1);
//                             sx1262_tx_payload = 0; // return 0 for invalid axis numbers
//                         }

//                         sx1262_tx_payload = (ACCEL_READ_OPCODE << 24) | (sx1262_param3 << 16) | (sx1262_tx_payload & 0xFFFF); // construct response payload with opcode 0x03, axis number in parameter 3, and axis value in lower 16 bits
//                         sx1262_transmit(sx1262_tx_payload); // transmit accelerometer read response
                        
//                         sx1262_get_status(&sx1262_chip_mode, &sx1262_command_status); // get status after transmission
//                         if (sx1262_command_status == RADIO_TX_DONE_STATUS) { // if command status is 0x06, the packet was transmitted and the radio is now in standby mode
//                             print_str("[LOG] - SX1262: Accelerometer read response transmitted\r\n");
//                         } else {
//                             print_str("[LOG] - SX1262: Error transmitting accelerometer read response, command status: 0x");
//                             print_hex(sx1262_command_status, 2, 1);
//                         } 
//                     }

//                     else if (sx1262_opcode == PD_READ_OPCODE) { // if opcode is 0x04, treat as photodiode read request
//                         print_str("[LOG] - SX1262: Received photodiode read request for channel ");
//                         print_dec_u16(sx1262_param3, 1); // parameter 3 contains the channel number to read
                        
//                         if (sx1262_param3 == 1) {
//                             sx1262_tx_payload = PD1;
//                         }
//                         else if (sx1262_param3 == 2) {
//                             sx1262_tx_payload = PD2;
//                         }
//                         else if (sx1262_param3 == 3) {
//                             sx1262_tx_payload = PD3;
//                         }
//                         else if (sx1262_param3 == 4) {
//                             sx1262_tx_payload = PD4;
//                         }
//                         else {
//                             print_str("[LOG] - SX1262: Invalid photodiode channel number in read request: ");
//                             print_dec_u16(sx1262_param3, 1);
//                             sx1262_tx_payload = 0; // return 0 for invalid channel numbers
//                         }

//                         sx1262_tx_payload = (PD_READ_OPCODE << 24) | (sx1262_param3 << 16) | (sx1262_tx_payload & 0xFFFF); // construct response payload with opcode 0x04, channel number in parameter 3, and channel value in lower 16 bits
//                         sx1262_transmit(sx1262_tx_payload); // transmit photodiode read response
                        
//                         sx1262_get_status(&sx1262_chip_mode, &sx1262_command_status); // get status after transmission
//                         if (sx1262_command_status == RADIO_TX_DONE_STATUS) { // if command status is 0x06, the packet was transmitted and the radio is now in standby mode
//                             print_str("[LOG] - SX1262: Photodiode read response transmitted\r\n");
//                         } else {
//                             print_str("[LOG] - SX1262: Error transmitting photodiode read response, command status: 0x");
//                             print_hex(sx1262_command_status, 2, 1);
//                         } 
//                     }
                    
//                     // ADD HANDLERS FOR OTHER OPCODES HERE
//                     else {
//                         print_str("[LOG] - SX1262: Received unknown command with opcode: 0x");
//                         print_hex(sx1262_opcode, 2, 1);
//                         print_str(" and parameters: 0x");
//                         print_hex(sx1262_param1, 2, 0);
//                         print_hex(sx1262_param2, 2, 0);
//                         print_hex(sx1262_param3, 2, 1);
//                     }
//                 }
                
//                 else {
//                     print_str("[LOG] - SX1262: Error receiving packet, chip mode: 0x");
//                     print_hex(sx1262_chip_mode, 2, 0);
//                     print_str(", Command status: 0x");
//                     print_hex(sx1262_command_status, 2, 0);
//                     sx1262_get_device_error(&sx1262_status, &sx1262_errors);
//                     print_str(", Device errors: 0x");
//                     print_hex(sx1262_errors, 4, 1);    
//                 }
//             } 
            
//             else {
//                 print_str("[LOG] - SX1262: no packet received\r\n");
//             }

//             if (read_timer() > 1000000) {
//                 set_counter(); // reset timer counter
            
//                 set_leds(led); // blink led
//                 led = ~led & 0x1;
                
//                 counter++; // increment counter
//                 print_str("[LOG] - SW: Timer tick - ");
//                 print_dec_u16(counter, 1);        
                
//                 print_str("[LOG] - SW: Main loop iteration complete\r\n");
//             }
//         }
//     }
// }



// some code to test SX1262 LoRa radio. This will be moved to a seperate function when we're done testing

// // test SPI communication with SX1262 LoRa radio. when done this will be moved to a seperate function
//             if (sx1262_sanity_check()) {
//                 print_str("[LOG] - SX1262: alive and responding over SPI\r\n");
//                 sx1262_transmit(vbat);
//                 sx1262_get_status(&sx1262_chip_mode, &sx1262_command_status);
//                 print_str("[LOG] - SX1262: transmitted packet\r\n");
//                 print_str("[LOG] - SX1262: Chip mode: 0x");
//                 print_hex(sx1262_chip_mode, 2, 0);
//                 print_str(", Command status: 0x");
//                 print_hex(sx1262_command_status, 2, 0);
//                 sx1262_get_device_error(&sx1262_status, &sx1262_errors);
//                 print_str(", Device errors: 0x");
//                 // print_hex(sx1262_status, 2, 0);
//                 print_hex(sx1262_errors, 4, 1);            
//             } else {
//                     print_str("[LOG] - SX1262: radio failure\r\n");
//                 }
                
//             print_str("[LOG] - SW: Main loop iteration complete\r\n");
//             }

// int main(void)
// {
//     set_counter();

//     // set SPI slaves to inactive (CS high)
//     digital_write(HIGH, GPO_ACCEL_CS_BIT);
//     digital_write(HIGH, GPO_LORA_CS_BIT);
//     digital_write(HIGH, GPO_ADC2_CS_BIT);
//     digital_write(HIGH, GPO_LORA_RF_SW_BIT); // set LoRa RF switch to RX
//     digital_write(HIGH, GPO_ADC2_SHDN_BIT); // set ADC2 to shutdown
//     digital_write(LOW, GPO_ADC2_CNV_BIT); // set ADC2 convert bit to 0
    
//     // configure the SPI controller
//     spi_configure(0, 0, 4); // clk_mode=0, data_mode=0, div=4 (0.75 MHz SPI clock for 12 MHz clock. 12MHz / (2^div))

//     // toggle HMC unit
//     set_hmc();

//     // initialize variables
//     unsigned int timer, led, vbat, axis_val = 0;
//     unsigned int ctrl_reg1_read = 0;

//     // photodiode outputs on ADC2 channels
//     unsigned int PD1, PD2, PD3, PD4 = 0;


//     while (1)
//     {
//         if (read_timer() > 1000000) {
//             set_counter();
//             set_leds(led);
//             led = ~led & 0x1;

//             if (sx1262_sanity_check()) {
//                 print_str("SX1262 sanity check passed!\r\n");
//             } else {
//                 print_str("SX1262 sanity check failed!\r\n");
//             }

//             // if (accel_sanity_check()) {
//             //     print_str("Accelerometer sanity check passed!\r\n");
//             //     accel_rw_register(0x20, 0x27, 0, 0);
//             //     ctrl_reg1_read = accel_rw_register(0x20, 0, 1, 0);
//             //     print_str("Control register 1: 0x");
//             //     print_hex(ctrl_reg1_read, 2, 1);
//             //     unsigned int x_lo = accel_rw_register(0x2C, 0, 1, 0);
//             //     unsigned int x_hi = accel_rw_register(0x2D, 0, 1, 0);
//             //     unsigned int x_val = (x_hi << 8) | x_lo;
//             //     print_str("X-axis value: 0x");
//             //     print_hex(x_val, 4, 1);
//             // } else {
//             //     print_str("Accelerometer sanity check failed!\r\n");
//             // }  
            
//             vbat = read_xadc();
//             print_str("Vbat: ");
//             print_dec_u16(vbat, 1);            

//             // read ADC2 channels for photodiode values
//             read_ADC2(&PD1, &PD2, &PD3, &PD4);
//             print_str("PD1: ");
//             print_dec_u16(PD1, 0);
//             print_str(" PD2: ");
//             print_dec_u16(PD2, 0);
//             print_str(", PD3: ");
//             print_dec_u16(PD3, 0);
//             print_str(", PD4: ");
//             print_dec_u16(PD4, 1);

//             print_str("HMC axis 1: dec16=");
//             axis_val = read_hmc_axis(1);
//             print_dec_u16(axis_val, 1);

//             print_str("HMC axis 2: dec16=");
//             axis_val = read_hmc_axis(2);
//             print_dec_u16(axis_val, 1);

//             print_str("HMC axis 3: dec16=");
//             axis_val = read_hmc_axis(3);
//             print_dec_u16(axis_val, 1);
//         }
//     }
// }


