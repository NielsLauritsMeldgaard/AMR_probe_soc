#include "../inc/hal.h"
#include "../inc/driver.h"


int main(void)
{
    set_counter();

    // set SPI slaves to inactive (CS high)
    digital_write(HIGH, GPO_ACCEL_CS_BIT);
    digital_write(HIGH, GPO_LORA_CS_BIT);
    digital_write(HIGH, GPO_ADC2_CS_BIT);
    digital_write(HIGH, GPO_LORA_RF_SW_BIT); // set LoRa RF switch to RX
    digital_write(HIGH, GPO_ADC2_SHDN_BIT); // set ADC2 to shutdown
    digital_write(LOW, GPO_ADC2_CNV_BIT); // set ADC2 convert bit to 0
    
    // configure the SPI controller
    spi_configure(0, 0, 4); // clk_mode=0, data_mode=0, div=4 (0.75 MHz SPI clock for 12 MHz clock. 12MHz / (2^div))

    // toggle HMC unit
    set_hmc();

    // initialize variables
    unsigned int timer = 0;
    unsigned int led = 0;

    // unsigned int ctrl_reg1 = 0x20; // CTRL_REG1 address for ILS328 accelerometer
    // unsigned int ctrl_reg1_value = 0x27;
    // unsigned int x_reg_lo = 0x28; // OUT_X_L register address for ILS328 accelerometer
    // unsigned int x_reg_hi = 0x29; // OUT_X_H register address for ILS328 accelerometer
    // unsigned int x_lo = 0;
    // unsigned int x_hi = 0;
    // unsigned int x_val = 0;

    unsigned int ctrl_reg1_read = 0;



    while (1)
    {
        if (read_timer() > 1000000) {
            set_counter();
            set_leds(led);
            led = ~led & 0x1;

            // if (sx1262_sanity_check()) {
            //     print_str("SX1262 sanity check passed!\r\n");
            // } else {
            //     print_str("SX1262 sanity check failed!\r\n");
            // }

            // if (accel_sanity_check()) {
            //     print_str("Accelerometer sanity check passed!\r\n");
            //     accel_rw_register(0x20, 0x27, 0, 0);
            //     ctrl_reg1_read = accel_rw_register(0x20, 0, 1, 0);
            //     print_str("Control register 1: 0x");
            //     print_hex(ctrl_reg1_read, 2, 1);
            //     unsigned int x_lo = accel_rw_register(0x28, 0, 1, 0);
            //     unsigned int x_hi = accel_rw_register(0x29, 0, 1, 0);
            //     unsigned int x_val = (x_hi << 8) | x_lo;
            //     print_str("X-axis value: 0x");
            //     print_hex(x_val, 4, 1);
            // } else {
            //     print_str("Accelerometer sanity check failed!\r\n");
            // }                                    
        }
    }
}


// safe for later
// // if (sx1262_sanity_check()) {
//             //     print_str("SX1262 sanity check passed!\r\n");
//             // } else {
//             //     print_str("SX1262 sanity check failed!\r\n");
//             // }

//             if (accel_sanity_check()) {
//                 print_str("Accelerometer sanity check passed!\r\n");
//                 // set in normal mode            
//                 accel_rw_register(0x20, 0x27, 0, 0); 
//                 // check that we can read back the value we just set
//                 print_str("Control register 1: 0x");
//                 ctrl_reg1_read = accel_rw_register(0x20, 0, 1, 0);
//                 print_hex(ctrl_reg1_read, 2, 1);

//                 // delay_cycles(100);
//                 // // read accel x-axis
//                 // x_lo = accel_rw_register(x_reg_lo, 0, 1, 0); // read OUT_X_L register
//                 // x_hi = accel_rw_register(x_reg_hi, 0, 1, 0); // read OUT_X_H register
//                 // // combine the low and high bytes to get the full 16-bit value
//                 // x_val = (x_hi << 8) | x_lo;
//                 // print_hex(x_val, 4, 1);
//             } else {
//                 print_str("Accelerometer sanity check failed!\r\n");
//             }



// int main() {
//     set_counter();

//     int timer = 0x0;
//     int counter = 0x0;
//     int xadc_value = 0x0;
//     int busy = 0x0;

//     spi_configure(0, 0, 120);
//     spi_set_slave(1);

//     while (1) {
//         read_timer(&timer);        
    
//         if (timer > 1000000) {
//             set_counter();
//             counter++;
//             print_str("Counter: 0x");
//             print_hex(counter, 4, 1); 
            
//             // if NOT busy
//             busy = spi_read_reg(SPI_STATUS_ADDR) & 0x1;
//             if (!busy) {
//                 // Write TX data 
//                 spi_write_reg(SPI_TX_DAT_ADDR, counter);

//                 // Trigger start pulse
//                 unsigned int ctrl = spi_read_reg(SPI_CNTRL_ADDR);
//                 spi_write_reg(SPI_CNTRL_ADDR, ctrl | 0x1);
//             }
//         }
//     }
//     return 0;
// }


// int main() {
//     set_counter();

//     // set SPI slaves to inactive (CS high)
//     digital_write(HIGH, GPO_ACCEL_CS_BIT);
//     digital_write(HIGH, GPO_LORA_CS_BIT);
//     digital_write(HIGH, GPO_ADC2_CS_BIT);

//     int timer = 0x0;
//     int led = 0x0;
//     int counter = 0x0;
//     int xadc_value = 0x0;


//     while (1) {
//         read_timer(&timer);
//         read_xadc(&xadc_value);
//         delay_cycles(10);
//         if (timer > 1000000) {
//             set_counter();
//             counter++;
//             print_str("Counter: 0x");
//             print_hex(counter, 4, 1);
//             print_str("XADC: 0x");
//             print_hex(xadc_value, 8, 1);
//             set_leds(led);
//             led = ~led;
//         }
//     }
//     return 0;
// }