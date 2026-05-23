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

    // initialize variables
    unsigned int timer = 0;
    unsigned int led = 0;



    while (1)
    {
        if (read_timer() > 1000000) {
            set_counter();
            set_leds(led);
            led = ~led & 0x1;
            if (sx1262_sanity_check()) {
                print_str("SX1262 sanity check passed!\r\n");
            } else {
                print_str("SX1262 sanity check failed!\r\n");
            }
        }
    }
}



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