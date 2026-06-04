#ifndef DRIVER_H
#define DRIVER_H

// Softcode for ADC2 configuration (see datasheet for LTC2357-16 from Analog Devices)
#define SOFTCODE 0b1011011011010000

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