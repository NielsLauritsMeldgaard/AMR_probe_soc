#include "oled.h"
#include "telemetry.h"

volatile bool eventOccurred = false;

/**
 * Initialize SSD1306 OLED display.
 * @return true if initialization succeeded, false otherwise
 */
bool init_display() {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) return false;
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Init...");
    display.display();
    return true;
}

/**
 * Interrupt handler for button press event (debounced).
 * Triggers a menu update flag if sufficient time has passed.
 */
void IRAM_ATTR incrementPtrEvent() {
    static volatile uint32_t lastMicros = 0;
    uint32_t now = micros();
    if (now - lastMicros > 500000) { 
        eventOccurred = true;
        lastMicros = now;
    }
}

/**
 * Update menu index if a button event has occurred.
 * @param menu_index Pointer to current menu index
 */
void update_menu_ptr(MenuIndex *menu_index) {
    if (eventOccurred) {
        *menu_index = (MenuIndex)(((*menu_index) + 1) % MENU_INDEX_COUNT);
        eventOccurred = false;
    }
}

/**
 * Render current telemetry page on OLED display.
 * @param menu_index Current menu selection
 * @param probe Latest sensor and radio data snapshot
 */
void render_menus(MenuIndex menu_index, Probe probe) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);

    switch(menu_index) {
        case MENU_INDEX_0: // Radio/System
            display.println("System:");
            display.printf("RSSI:%d\n", (int)probe.radio.RSSI);
            display.printf("SNR :%d\n", (int)probe.radio.SNR);
            display.printf("Bat :%.2fV\n", probe.sensorData.batteryVoltage);
            break;
            
        case MENU_INDEX_1: // HMC
            display.println("Mag-meter:");
            // display.printf("H1:%d\n", probe.sensorData.mag.axis1);
            // display.printf("H2:%d\n", probe.sensorData.mag.axis2);
            // display.printf("H3:%d\n", probe.sensorData.mag.axis3);
            display.printf("H1:%.4f\n", probe.sensorData.mag.axis1_G);
            display.printf("H2:%.4f\n", probe.sensorData.mag.axis2_G);
            display.printf("H3:%.4f\n", probe.sensorData.mag.axis3_G);
            break;

        case MENU_INDEX_2: // Accel
            display.println("Accel(g):");
            display.printf("X:%.2f\n", probe.sensorData.accel.x);
            display.printf("Y:%.2f\n", probe.sensorData.accel.y);
            display.printf("Z:%.2f\n", probe.sensorData.accel.z);
            break;

        case MENU_INDEX_3: // Photodiodes
            display.println("Diodes:");
            display.setTextSize(1);
            display.printf("PD1:%u\n", probe.sensorData.photodiodes.PD[0]);
            display.printf("PD2:%u\n", probe.sensorData.photodiodes.PD[1]);
            display.printf("PD3:%u\n", probe.sensorData.photodiodes.PD[2]);
            display.printf("PD4:%u\n", probe.sensorData.photodiodes.PD[3]);
            display.setTextSize(2);
            display.printf("AZ:%.1f\n", probe.sensorData.photodiodes.azimuth);
            break;

        case MENU_INDEX_4: // Failures
            display.println("Failures:");
            if (probe.radio.state == 0) display.println("RADIO OK");
            else display.printf("ERR:0x%02X\n", probe.radio.state);
            break;
    }
    display.display();
}