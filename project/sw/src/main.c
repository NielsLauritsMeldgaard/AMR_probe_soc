#include "../inc/hal.h"
#include "../inc/driver.h"

int main() {
    set_counter();

    int timer = 0x0;
    int counter = 0x0;
    // int xadc_value = 0x0;
    // int busy = 0x0;

    // spi_configure(0, 0, 120);
    // spi_set_slave(1);

    while (1) {
        read_timer(&timer);        
    
        if (timer > 1000000) {
            set_counter();
            counter++;
            print_str("Counter: 0x");
            print_hex(counter, 4, 1); 
            
            // if NOT busy
            // busy = spi_read_reg(SPI_STATUS_ADDR) & 0x1;
            // if (!busy) {
            //     // Write TX data 
            //     spi_write_reg(SPI_TX_DAT_ADDR, counter);

            //     // Trigger start pulse
            //     unsigned int ctrl = spi_read_reg(SPI_CNTRL_ADDR);
            //     spi_write_reg(SPI_CNTRL_ADDR, ctrl | 0x1);
            // }
        }
    }
    return 0;
}


// int main() {
//     set_counter();

//     int timer = 0x0;
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
//         }
//     }
//     return 0;
// }