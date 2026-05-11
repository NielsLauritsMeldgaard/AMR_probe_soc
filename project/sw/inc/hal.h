#ifndef HAL_H
#define HAL_H

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

// Currently 3-slaves supported

void set_leds(unsigned int value);
void delay_cycles(unsigned int cycles);
void uart_write_byte(unsigned int c);
void set_counter(void);
void read_timer(unsigned int *value);
void read_xadc(unsigned int *value);
void spi_write_reg(unsigned int addr, unsigned int value);
unsigned int spi_read_reg(unsigned int addr);
void spi_configure(unsigned int clk_mode, unsigned int data_mode, unsigned int div);
void spi_set_slave(unsigned int slave);
void spi_clear_slave(unsigned int slave);

#endif // HAL_H
