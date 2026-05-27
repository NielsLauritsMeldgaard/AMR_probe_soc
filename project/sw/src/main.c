#include "../inc/hal.h"
#include "../inc/driver.h"
#include "../inc/sx1262_driver.h"

int main(void)
{
    set_counter();

    // set SPI slaves to inactive (CS high)
    digital_write(HIGH, GPO_ACCEL_CS_BIT);
    digital_write(HIGH, GPO_LORA_CS_BIT);
    digital_write(HIGH, GPO_ADC2_CS_BIT);
    digital_write(HIGH, GPO_LORA_RF_SW_BIT);    // Set HIGH and let DIO2 control RF switch for TX/RX
    digital_write(HIGH, GPO_ADC2_SHDN_BIT);     // set ADC2 to shutdown
    digital_write(LOW, GPO_ADC2_CNV_BIT);       // set ADC2 convert bit to 0
    
    // configure the SPI controller
    spi_configure(0, 0, 4); // clk_mode=0, data_mode=0, div=4 (0.75 MHz SPI clock for 12 MHz clock. 12MHz / (2^div))

    // initialize variables
    unsigned int timer = 0; 
    unsigned int led = 0; 
    unsigned int counter = 0; 
    unsigned int vbat = 0;
    // initialize lora radio
    sx1262_configure_essentials();


    while (1)
    {
        if (read_timer() > 1000000) {               
            set_counter(); // reset timer counter
            
            set_leds(led); // blink led
            led = ~led & 0x1;

            sx1262_set_pa_config();
            
            counter++; // increment counter
            print_str("[LOG] - Timer tick - ");
            print_dec_u16(counter, 1);        
            
            vbat = read_xadc(); // read battery voltage from XADC
            print_str("[LOG] - battery voltage: ");
            print_dec_u16(vbat, 1);

            print_str("[LOG] - HMC axis 1: ");
            print_dec_u16(read_hmc_axis(1), 1);

            print_str("[LOG] - HMC axis 2: ");
            print_dec_u16(read_hmc_axis(2), 1);

            print_str("[LOG] - HMC axis 3: ");
            print_dec_u16(read_hmc_axis(3), 1);

            // // test SPI communication with SX1262 LoRa radio. when done this will be moved to a seperate function
            // if (sx1262_sanity_check()) {

            //     print_str("[LOG] - SX1262: Transmitting packet\r\n");
            //     sx1262_transmit(counter); // transmit the counter value as the payload in a LoRa packet
            //     print_str("[LOG] - SX1262: going into standby mode\r\n");
            //     sx1262_set_power_mode(STANDBY_RC_MODE);     

            // } else {
            //     print_str("[LOG] - SX1262: radio failure\r\n");
            // }

            // print_str("[LOG] - Main loop iteration complete\r\n");
        }
    }
}

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


