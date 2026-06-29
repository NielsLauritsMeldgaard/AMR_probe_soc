#include "../inc/hal.h"
#include "../inc/driver.h"
#include "../inc/sx1262_driver.h"

// Opcode Definitions for the user defined command interface
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
void print_csv(unsigned int v, int h1, int h2, int h3, int a1, int a2, int a3, int p1, int p2, int p3, int p4, int dac_off, int timer) {
    // Format: BAT, HMC1, HMC2, HMC3, ACCEL1, ACCEL2, ACCEL3, PD1, PD2, PD3, PD4, dac_off, timer
    print_dec_u16(v, 0);  print_str(",");
    print_hex(h1, 4, 0); print_str(",");
    print_hex(h2, 4, 0); print_str(",");
    print_hex(h3, 4, 0); print_str(",");
    // print_dec_u16(h1, 0); print_str(",");
    // print_dec_u16(h2, 0); print_str(",");
    // print_dec_u16(h3, 0); print_str(",");
    print_hex(a1, 4, 0); print_str(",");
    print_hex(a2, 4, 0); print_str(",");
    print_hex(a3, 4, 0); print_str(",");
    print_dec_u16(p1, 0); print_str(",");
    print_dec_u16(p2, 0); print_str(",");
    print_dec_u16(p3, 0); print_str(",");
    print_dec_u16(p4, 0); print_str(","); // Ends with \r\n
    print_dec_u16(dac_off, 0); print_str(",");// Print DAC offset state
    print_dec_u16(timer, 1); // Print timer state

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

    // Radio Boot
    digital_write(LOW, GPO_LORA_RESET_BIT);
    delay_cycles(10000);
    digital_write(HIGH, GPO_LORA_RESET_BIT);
    sx1262_wait_while_busy();
    sx1262_configure_essentials();
    
    // Check if SX1262 is present and responding
    if (!sx1262_sanity_check()) {
        while (1)
        {
            print_str("[LOG] FATAL: SX1262 not found!\r\n");
        }        
    }
    print_str("[LOG] SX1262 online. System Ready.\r\n");

    // Check if accelerometer is present and responding
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
    unsigned int timer_val = 0;

    // Main loop
    while (1) {
        btn_state = read_button();
        if (btn_state == 1 && btn_state_prev == 0) {
            dac_off = !dac_off; // Toggle DAC state
            if (!dac_off) {
                set_leds(3); // Set both LEDs on to indicate DAC is active (example)
            } else {
                set_leds(0); // Set both LEDs off to indicate DAC is off
            }
        }
        btn_state_prev = btn_state;

        // Update Sensors
        vbat = read_xadc();
        h1 = read_hmc_axis(1) - 32768; h2 = read_hmc_axis(2) - 32768; h3 = read_hmc_axis(3) - 32768;
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

        // Periodic Logging (every 15 ms)
        timer_val = read_timer();
        if (timer_val > 15000) {
            // Reset timer and print CSV log
            print_csv(vbat, h1, h2, h3, ax, ay, az, pd1, pd2, p3, pd4, dac_off, timer_val);
            set_counter();

            // Check for device errors, print if any
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



