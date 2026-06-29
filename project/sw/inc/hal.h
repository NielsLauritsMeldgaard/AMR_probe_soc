#ifndef HAL_H
#define HAL_H

// Addresses for memory-mapped I/O
#define IO_BASE         0x40000000u
#define LED_ADDR        (IO_BASE + 0x0)
#define TIMER_ADDR      (IO_BASE + 0x4)
#define UART_ADDR       (IO_BASE + 0x8)
#define BUTTONS_ADDR    (IO_BASE + 0xc)
#define SPI_BASE_ADDR   (IO_BASE + 0x10)
#define SPI_CNTRL_ADDR  (SPI_BASE_ADDR + 0x0)
#define SPI_STATUS_ADDR (SPI_BASE_ADDR + 0x4)
#define SPI_TX_DAT_ADDR (SPI_BASE_ADDR + 0x8)
#define SPI_RX_DAT_ADDR (SPI_BASE_ADDR + 0xC)
#define XADC_ADDR       (IO_BASE + 0x20)
#define GPO_ADDR        (IO_BASE + 0x24)
#define GPI_ADDR        (IO_BASE + 0x28)
#define HMC_START_ADDR  (IO_BASE + 0x2C)
#define HMC_1_OUT_ADDR  (IO_BASE + 0x30)
#define HMC_2_OUT_ADDR  (IO_BASE + 0x34)
#define HMC_3_OUT_ADDR  (IO_BASE + 0x38)

// Define a bool type
#define HIGH  0x1
#define LOW   0x0

// GPIO bit definitions for GPO register
//          Output                 ||       BIT         ||      IO       ||
    //=========================================================================
#define     GPO_ACCEL_CS_BIT                 0          //     P08      
#define     GPO_LORA_CS_BIT                  1          //     P14
#define     GPO_LORA_RF_SW_BIT               2          //     P21
#define     GPO_ADC2_CS_BIT                  3          //     P39
#define     GPO_ADC2_SHDN_BIT                4          //     P48
#define     GPO_ADC2_CNV_BIT                 5          //     P47
#define     GPO_JA7_BIT                      6          //     JA07
#define     GPO_JA8_BIT                      7          //     JA08
#define     GPO_JA9_BIT                      8          //     JA09
#define     GPO_JA10_BIT                     9          //     JA10
#define     GPO_LORA_RESET_BIT               10         //     P17

// GPIO bit definitions for GPI register
//          Input                  ||       BIT         ||      IO       ||
//=========================================================================
#define     GPI_ACCEL_INT1_BIT               0          //     P13
#define     GPI_ACCEL_INT2_BIT               1          //     P12
#define     GPI_LORA_DIO1_BIT                2          //     P23
#define     GPI_LORA_BUSY_BIT                3          //     P22
#define     GPI_ADC2_BUSY_BIT                4          //     P40
#define     GPI_JA1_BIT                      5          //     JA01
#define     GPI_JA2_BIT                      6          //     JA02
#define     GPI_JA3_BIT                      7          //     JA03
#define     GPI_JA4_BIT                      8          //     JA04


void set_leds(unsigned int value);
void delay_cycles(unsigned int cycles);
void uart_write_byte(unsigned int c);
void set_counter();
unsigned int read_timer();
unsigned int read_xadc();
void spi_write_reg(unsigned int addr, unsigned int value);
unsigned int spi_read_reg(unsigned int addr);
void digital_write(unsigned int value, unsigned int IO_BIT);
unsigned int digital_read(unsigned int IO_BIT);
void set_hmc();
unsigned int read_hmc_axis(unsigned int axis);
int read_button();

#endif // HAL_H
