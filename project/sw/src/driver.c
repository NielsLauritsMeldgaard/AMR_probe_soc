#include "../inc/hal.h"
#include "../inc/driver.h"

/**
 * Write a 32-bit hexadecimal value to the UART.
 * @param value The value to write
 * @param nibbles The number of nibbles to display
 * @param newline Whether to add a newline character
 */
void print_hex(unsigned int value, unsigned int nibbles, unsigned int newline) {
    for (int i = (nibbles - 1); i >= 0; i--) {
        unsigned int nibble = (value >> (i * 4)) & 0xF;
        if (nibble < 10)
            uart_write_byte('0' + nibble);
        else
            uart_write_byte('A' + (nibble - 10));
    }
    if (newline) {
        uart_write_byte('\r'); // Optional: newline for readability
        uart_write_byte('\n');
    }

}

void print_str(const char *str)
{
    while (*str)
    {
        uart_write_byte(*str);
        str++;
    }
}

// void printf()