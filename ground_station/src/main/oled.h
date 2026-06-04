#ifndef OLED_H
#define OLED_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define BTN_PIN 10

typedef enum {
    MENU_INDEX_0,
    MENU_INDEX_1,
    MENU_INDEX_2,
    // Add more menu indices as needed
} MenuIndex;

bool init_display();
void update_menu_ptr(int *menu_index);
void IRAM_ATTR incrementPtrEvent();



#endif