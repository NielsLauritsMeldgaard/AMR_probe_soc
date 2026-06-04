#include "telemetry.h"
#include "oled.h"



Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Probe probe;
Command current_tx_cmd;
Command current_rx_cmd;
unsigned long lastUpdateCycle = 0;
int menuIndex = 0;

// DYNAMIC TASK LIST: Add or remove commands here to change what the GS polls
// CommandType sensorTasks[] = { GET_BAT_VOLTAGE, GET_HMC_AXIS_1, GET_HMC_AXIS_2, GET_HMC_AXIS_3, GET_ACCEL_X, GET_ACCEL_Y, GET_ACCEL_Z, GET_PD1, GET_PD2, GET_PD3, GET_PD4 };
CommandType sensorTasks[] = { GET_HMC_AXIS_3 };
const int totalTasks = sizeof(sensorTasks) / sizeof(sensorTasks[0]);
int currentTaskIndex = 0;
bool isSequenceRunning = false;

void setup() {
  Serial.begin(115200);
  while(!Serial); 
  init_radio();
  memset(&probe, 0, sizeof(Probe));
  Wire.begin(8, 9); // SDA, SCL
  if (!init_display()) {
    Serial.println(F("SSD1306 allocation failed"));  
  }

  pinMode(BTN_PIN, INPUT_PULLUP);
  attachInterrupt(
        digitalPinToInterrupt(BTN_PIN), // Pin 10 is connected to menu select pushbutton
        incrementPtrEvent,
        RISING
    );
  
}

void loop() {
  unsigned long currentTime = millis();

  if (!isSequenceRunning && (currentTime - lastUpdateCycle >= INTERVAL_MS)) {
    isSequenceRunning = true;
    currentTaskIndex = 0;
    lastUpdateCycle = currentTime;
    //Serial.println(F("\n[LOG] Starting new command sequence..."));
  }

  if(isSequenceRunning) {
    buildCommand(&current_tx_cmd, sensorTasks[currentTaskIndex]); 
    
    // perform transaction for current command
    long result = radio_transaction_master_async(&probe, &current_tx_cmd);
    
    if (result > 0) {
      //SUCCESS
      parseCommand(result, &current_rx_cmd);
      process_sensor_data(&probe, &current_rx_cmd);
      currentTaskIndex++; // Move to the next task in the list
      delay(10);
    }
    else if (result < 0) {
      // FAILURE (Timeout or hardware error)
      Serial.printf("[LOG] Task %d (Op: 0x%02X) failed. ", 
                    currentTaskIndex, current_tx_cmd.opcode);
      printRadioStatus(&probe.radio);
      currentTaskIndex++; // Move on so we don't hang the loop
      delay(10);
    }

    //check if we finished the list
    if (currentTaskIndex >= totalTasks) {
      isSequenceRunning = false;
      
      //printRadioStatus(&probe.radio);
      Serial.println(probe.sensorData.mag.axis3);
            
      // Serial.println(F("[LOG] Command sequence complete."));

    }
  }

  update_menu_ptr(&menuIndex);

  // // SCHEDULER: Start heartbeat if it's time and no transaction is active
  // if (!probe.radio.isTransactionActive) {
  //   if (currentTime - lastHeartbeatTime >= INTERVAL_MS) {
  //     Serial.println(F("\n[LOG] Sending Heartbeat Request..."));
  //     parseCommand((uint32_t)HEARTBEAT_OPCODE << 24, &tx_cmd);
  //     radio_transaction_master_async(&probe, &tx_cmd);
  //     lastHeartbeatTime = currentTime;
  //   }
  // } 
  
  // // POLLING: Process the active transaction
  // if (probe.radio.isTransactionActive) {
  //   int result = radio_transaction_master_async(&probe, &tx_cmd);
    
  //   if (result > 0) {
  //     // result contains the raw VBat from bytes 2-3
  //     if (tx_cmd.opcode != HEARTBEAT_OPCODE) {
  //       // Handle if we receive a response for a different command than expected
  //       Serial.print(F("[LOG] Received response for unexpected opcode: 0x"));
  //       Serial.println(tx_cmd.opcode, HEX);
  //     } else {
  //       probe.sensorData.batteryVoltage = (float)result / 4095.0 / k_VBAT;
        
  //       Serial.print(F("[LOG] Success! Battery voltage: "));
  //       Serial.print(probe.sensorData.batteryVoltage);
  //       Serial.println(F(" V"));
  //       printRadioStatus(&probe.radio);
  //     } 
  //   } else if (result < 0) {
  //     Serial.println(F("[LOG] Transaction Failed"));
  //     printRadioStatus(&probe.radio);
  //   }
  // }

  // OTHER TASKS: add OLED code here etc here
  // updateOLED(&probe);
}


// #include <RadioLib.h>
// #include "telemetry"

// SX1262 radio = new Module(7, 1, 3, 2);

// #define HEARTBEAT_INTERVAL_MS 10000 
// #define RESPONSE_TIMEOUT_MS 2000 
// #define HEARTBEAT_OPCODE 0x01


// #define PAYLOAD_LEN_BYTES 4 
// #define k 0.036

// volatile bool receivedFlag = false;

// typedef struct {
//   uint8_t opcode;
//   uint8_t param1;
//   uint8_t param2;
//   uint8_t param3;
// } Command;

// typedef struct {
//   int x;
//   int y;
//   int z;
// } Accelerometer;

// typedef struct {
//   int16_t axis1;
//   int16_t axis2;
//   int16_t axis3;
// } Magnetometer;

// typedef struct {
//   int PD1;
//   int PD2;
//   int PD3;
//   int PD4;
// } Photodiodes;

// typedef struct {
//   int RSSI;
//   int SNR;
//   int frequencyError;
//   int state;
// } Radio;

// typedef struct {
//   float batteryVoltage;
//   Photodiodes photodiodes;
//   Accelerometer accel;
//   Magnetometer mag;
// } SensorData;


// typedef struct {
//   Command lastCommand;
//   SensorData lastSensorData;
//   Radio lastRadioStatus;
// } Probe;

// #if defined(ESP8266) || defined(ESP32)
//   ICACHE_RAM_ATTR
// #endif
// void setFlag(void) {
//   receivedFlag = true;
// }

// enum State { IDLE, MONITOR };
// State currentState = IDLE;

// // Track if we are currently waiting for the FPGA to answer a specific request
// bool isTransactionActive = false; 

// void setup() {
//   Serial.begin(9600);
//   int state = radio.begin(868.0, 125.0, 9, 7, 0x12, 10, 8);  
//   if (state != RADIOLIB_ERR_NONE) { while (true); }

//   radio.setCRC(true);           
//   radio.invertIQ(false);        
//   radio.setDio2AsRfSwitch(true);
//   radio.setPacketReceivedAction(setFlag);
// }

// // Returns: 1 on success, -1 on failure/timeout, 0 if still waiting
// int radio_transaction_master_async(int opcode, uint8_t* param) {
//   static unsigned long startTime = 0;
//   uint8_t txBuffer[4] = {0};

//   if (!isTransactionActive) {
//     // START PHASE
//     txBuffer[0] = opcode; 
//     if (param != NULL) {
//       txBuffer[1] = param[0]; txBuffer[2] = param[1]; txBuffer[3] = param[2];
//     }

//     int state = radio.transmit(txBuffer, 4);
//     receivedFlag = false; 

//     if (state == RADIOLIB_ERR_NONE) {
//       isTransactionActive = true;
//       startTime = millis();
//       radio.startReceive(); // CRITICAL: Start listening for the reply!
//       return 0; 
//     } else {
//       return -1;
//     }
//   } else {
//     // WAITING PHASE
//     if (receivedFlag) {
//       isTransactionActive = false;
//       receivedFlag = false;
      
//       byte byteArr[4];
//       if (radio.readData(byteArr, 4) == RADIOLIB_ERR_NONE) {
//         if (byteArr[0] == opcode) {
//           // Success! Return the 16-bit value from bytes 2 and 3
//           return (byteArr[2] << 8) | byteArr[3];
//         }
//       }
//       return -1;
//     }

//     if (millis() - startTime > RESPONSE_TIMEOUT_MS) {
//       isTransactionActive = false;
//       return -1; // Timeout
//     }

//     return 0; // Still waiting
//   }
// }

// void handle_idle_state() {
//   static unsigned long last_heartbeat_time = 0;
//   unsigned long current_time = millis();

//   // If we aren't doing anything, check if it's time to start a heartbeat
//   if (!isTransactionActive) {
//     if (current_time - last_heartbeat_time >= HEARTBEAT_INTERVAL_MS) {
//       Serial.println(F("[LOG] - Starting Heartbeat Request..."));
//       radio_transaction_master_async(HEARTBEAT_OPCODE, NULL);
//       last_heartbeat_time = current_time; // Reset timer
//     }
//   } 
//   // If we ARE in a transaction, keep polling it
//   else {
//     int result = radio_transaction_master_async(HEARTBEAT_OPCODE, NULL);
    
//     if (result > 0) {
//       // Success
//       float voltage = (float)result / 4095.0 / k;
//       Serial.print(F("[LOG] - Heartbeat Success! battery voltage: "));
//       Serial.print(voltage);
//       Serial.println(F("V"));
//     } 
//     else if (result < 0) {
//       // Failure or Timeout
//       Serial.println(F("[LOG] - Heartbeat Failed (Timeout or Error)"));
//     }
//     // if result == 0, we do nothing and wait for next loop iteration
//   }
// }

// void loop() {
//   switch (currentState) {
//     case IDLE:
//       handle_idle_state();
//       break;
//   }
// }