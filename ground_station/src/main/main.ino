#include "telemetry.h"
#include "oled.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Probe probe;
Probe probe_buf; 
Command current_tx_cmd;
Command current_rx_cmd;
unsigned long lastUpdateCycle = 0;
unsigned long lastMenuPrint = 0; // Timer for occasional menu refresh

// State management
MenuIndex menuIndex = MENU_INDEX_0;
bool isLoggingActive = false; 

// Sequences
CommandType batterySeq[] = { GET_BAT_VOLTAGE };
CommandType hmcSeq[]     = { GET_HMC_AXIS_1, GET_HMC_AXIS_2, GET_HMC_AXIS_3 };
CommandType accelSeq[]   = { GET_ACCEL_X, GET_ACCEL_Y, GET_ACCEL_Z };
CommandType pdSeq[]      = { GET_PD1, GET_PD2, GET_PD3, GET_PD4 };

CommandType *activeSequence = batterySeq;
size_t totalTasks = 1;
int currentTaskIndex = 0;
bool isSequenceRunning = false;

/**
 * Clear UART terminal screen and reset cursor position.
 */
void clearTerminal() {
    Serial.print(F("\033[2J\033[H"));
}

/**
 * Print CSV header for selected logging mode.
 * @param index Selected menu index defining dataset type
 */
void printCsvHeader(MenuIndex index) {
    clearTerminal();
    Serial.print(F("--- LOG START ---\r\n"));
    switch(index) {
        case MENU_INDEX_0:
            Serial.println(F("Time(f),SNR(f),RSSI(f),Voltage(f)"));
            break;
        case MENU_INDEX_1:
            Serial.println(F("Time(f),H1(i16),H2(i16),H3(i16),H1(f),H2(f),H3(f)"));
            break;
        case MENU_INDEX_2:
            Serial.println(F("Time(f),X(g),Y(g),Z(g),Pitch(deg),Roll(deg)"));
            break;
        case MENU_INDEX_3:
            Serial.println(F("Time(f),PD1(u16),PD2(u16),PD3(u16),PD4(u16),Azimuth(f)"));
            break;
        case MENU_INDEX_4:
            Serial.println(F("Time(f),RadioState(i)"));
            break;
    }
}

/**
 * Print main UART menu for selecting logging mode.
 */
void print_main_menu() {
    clearTerminal();
    Serial.println(F("----- LOG MENU -----"));
    Serial.println(F("0: System (SNR/RSSI/BAT)"));
    Serial.println(F("1: HMC (Magnetometer)"));
    Serial.println(F("2: Accelerometer (G/Ori)"));
    Serial.println(F("3: Photodiodes (PD/Azimuth)"));
    Serial.println(F("4: Failures (Radio Status)"));
    Serial.println(F("\r\nAction: Press 0-4 to start logging."));
    Serial.println(F("Control: Press 'q' to stop/return."));
    Serial.println(F("-----------------------------"));
    lastMenuPrint = millis();
}

/**
 * Arduino setup function. Initializes peripherals and UI.
 */
void setup() {
    Serial.begin(115200);
    while(!Serial);
    init_radio();
    memset(&probe, 0, sizeof(Probe));
    memset(&probe_buf, 0, sizeof(Probe));
    Wire.begin(8, 9); 
    if (!init_display()) {
        Serial.println(F("SSD1306 allocation failed"));
    }

    pinMode(BTN_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_PIN), incrementPtrEvent, RISING);
    
    print_main_menu();
}

/**
 * Handle UART input for menu selection and control.
 */
void handleUartMenu() {
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (c >= '0' && c <= '4') {
            menuIndex = (MenuIndex)(c - '0');
            isLoggingActive = true;
            isSequenceRunning = false;
            printCsvHeader(menuIndex);
        } else if (c == 'q' || c == 'Q') {
            isLoggingActive = false;
            isSequenceRunning = false;
            print_main_menu();
        }
    }
}

/**
 * Main application loop.
 */
void loop() {
    unsigned long currentTime = millis();
    handleUartMenu();

    if (!isLoggingActive && (currentTime - lastMenuPrint > 5000)) {
        print_main_menu();
    }

    MenuIndex oldIndex = menuIndex;
    update_menu_ptr(&menuIndex);
    if (menuIndex != oldIndex) {
        isLoggingActive = true;
        isSequenceRunning = false;
        printCsvHeader(menuIndex);
    }

    switch (menuIndex) {
        case MENU_INDEX_0: activeSequence = batterySeq; totalTasks = 1; break;
        case MENU_INDEX_1: activeSequence = hmcSeq;     totalTasks = 3; break;
        case MENU_INDEX_2: activeSequence = accelSeq;   totalTasks = 3; break;
        case MENU_INDEX_3: activeSequence = pdSeq;      totalTasks = 4; break;
        case MENU_INDEX_4: activeSequence = batterySeq; totalTasks = 1; break;
    }

    if (isLoggingActive) {
        if (!isSequenceRunning && (currentTime - lastUpdateCycle >= INTERVAL_MS)) {
            isSequenceRunning = true;
            currentTaskIndex = 0;
            lastUpdateCycle = currentTime;
        }

        if(isSequenceRunning) {
            buildCommand(&current_tx_cmd, activeSequence[currentTaskIndex]);
            long result = radio_transaction_master_async(&probe_buf, &current_tx_cmd);

            if (result > 0) {
                parseCommand(result, &current_rx_cmd);
                process_sensor_data(&probe_buf , &current_rx_cmd);
                currentTaskIndex++;
            }
            else if (result < 0) {
                currentTaskIndex++; 
            }

            if (currentTaskIndex >= totalTasks) {
                isSequenceRunning = false;
                memcpy(&probe, &probe_buf, sizeof(Probe));

                Serial.print((millis() / 1000.0), 3);
                Serial.print(",");

                switch(menuIndex) {
                    case MENU_INDEX_0:
                        Serial.printf("%.1f,%.1f,%.2f\r\n", probe.radio.SNR, probe.radio.RSSI, probe.sensorData.batteryVoltage);
                        break;
                    case MENU_INDEX_1:
                        Serial.printf("%d,%d,%d,%.12f,%.12f,%.12f\r\n", probe.sensorData.mag.axis1, probe.sensorData.mag.axis2, probe.sensorData.mag.axis3, probe.sensorData.mag.axis1_G, probe.sensorData.mag.axis2_G, probe.sensorData.mag.axis3_G);
                        break;
                    case MENU_INDEX_2:
                        Serial.printf("%.3f,%.3f,%.3f,%.2f,%.2f\r\n", probe.sensorData.accel.x, probe.sensorData.accel.y, probe.sensorData.accel.z, 
                                      probe.sensorData.accel.pitch * 180/PI, probe.sensorData.accel.roll * 180/PI);
                        break;
                    case MENU_INDEX_3:
                        Serial.printf("%u,%u,%u,%u,%.2f\r\n", probe.sensorData.photodiodes.PD[0], probe.sensorData.photodiodes.PD[1], 
                                      probe.sensorData.photodiodes.PD[2], probe.sensorData.photodiodes.PD[3], probe.sensorData.photodiodes.azimuth);
                        break;
                    case MENU_INDEX_4:
                        Serial.println(probe.radio.state);
                        break;
                }
            }
        }
    }

    static unsigned long lastRenderTime = 0;
    if (currentTime - lastRenderTime >= 33) { 
        render_menus(menuIndex, probe);
        lastRenderTime = currentTime;
    }
}