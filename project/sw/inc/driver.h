#ifndef DRIVER_H
#define DRIVER_H


void print_hex(unsigned int value, unsigned int nibbles, unsigned int newline);
void print_str(const char *str);
void spi_configure(unsigned int clk_mode, unsigned int data_mode, unsigned int div);
unsigned int spi_transfer(unsigned int tx);
unsigned int sx1262_sanity_check();


#endif // DRIVER_H