#ifndef DRIVER_H
#define DRIVER_H

// Softcode for ADC2 configuration (see datasheet for LTC2357-16 from Analog Devices)
#define SOFTCODE 0b1011011011010000

// accel settings
#define PM 0b110 // power mode: 0b11 low-power, 10Hz output rate (3-bits)
#define DR 0b00  // data rate (computed from LPF): 0b00 = 50Hz, 0b01 = 100Hz, 0b10 = 400Hz, 0b11 = 1000Hz (2-bits)
#define EN_X 0b1 // enable X axis
#define EN_Y 0b1 // enable Y axis
#define EN_Z 0b1 // enable Z axis
// construct 8-bit control register value from settings
#define CTRL_REG1_VALUE ((PM << 5) | (DR << 3) | (EN_X << 2) | (EN_Y << 1) | (EN_Z << 0))
#define CTRL_REG1_ADDR 0x20

void print_hex(unsigned int value, unsigned int nibbles, unsigned int newline);
void print_dec_u16(unsigned int v, unsigned int newline);
void print_str(const char *str);
void spi_configure(unsigned int clk_mode, unsigned int data_mode, unsigned int div);
unsigned int spi_transfer(unsigned int tx);
unsigned int accel_rw_register(unsigned int regAddress, unsigned int value, unsigned int RW, unsigned int MS);
unsigned int read_accel_axis(unsigned int axis);
unsigned int accel_sanity_check();
void read_ADC2(unsigned int *ch0, unsigned int *ch1, unsigned int *ch2, unsigned int *ch3);


#endif // DRIVER_H