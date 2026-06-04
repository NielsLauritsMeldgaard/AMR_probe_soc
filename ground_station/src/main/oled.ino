#include "oled.h"

volatile bool eventOccurred = false;

bool init_display() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // Serial.println(F("SSD1306 allocation failed"));
    return false;
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Hello DTU!");
  display.display();
  return true;
}


void IRAM_ATTR incrementPtrEvent() {
    static volatile uint32_t lastMicros = 0;
    uint32_t now = micros();

    if (now - lastMicros > 2000) { // 2 ms debounce
        eventOccurred = true;
        lastMicros = now;
    }
}


void update_menu_ptr(int *menu_index) {

    if (eventOccurred ) {
        (*menu_index)++;
        eventOccurred = false;
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Menu Index:");
        display.println(*menu_index);
        display.display();
    }

}