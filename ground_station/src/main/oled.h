#ifndef OLED_H
#define OLED_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define BTN_PIN 10

typedef enum {
    MENU_INDEX_0, // System
    MENU_INDEX_1, // HMC
    MENU_INDEX_2, // Accel
    MENU_INDEX_3, // Photodiodes
    MENU_INDEX_4, // Failures
    MENU_INDEX_COUNT
} MenuIndex;

bool init_display();
void update_menu_ptr(MenuIndex *menu_index);
void IRAM_ATTR incrementPtrEvent();
void render_menus(MenuIndex menu_index, Probe probe);

#endif