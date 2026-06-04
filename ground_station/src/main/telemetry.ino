#include "telemetry.h"

// Instantiate global hardware objects
SX1262 radio = new Module(7, 1, 3, 2);
volatile bool receivedFlag = false;

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  receivedFlag = true;
}


void init_radio() {
    Serial.print(F("[SX1262] Initializing... "));
    // Freq: 868.0, BW: 125.0, SF: 9, CR: 7, Sync: 0x12, Pwr: 10, Preamble: 8
    int state = radio.begin(868.0, 125.0, 9, 7, 0x12, 10, 8);  
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
    } else {
        Serial.printf("failed, code %d\n", state);
        while (true);
    }
    
    radio.setCRC(true);           
    radio.invertIQ(false);        
    radio.setDio2AsRfSwitch(true);
    radio.setPacketReceivedAction(setFlag);
}


long radio_transaction_master_async(Probe* probe, Command* cmd) {
  static unsigned long startTime = 0;
  uint8_t txBuffer[4] = {0};

  if (!probe->radio.isTransactionActive) {
    // --- START PHASE ---
    txBuffer[0] = cmd->opcode; 
    txBuffer[1] = cmd->params[2]; 
    txBuffer[2] = cmd->params[1]; 
    txBuffer[3] = cmd->params[0];

    // 1. Perform the blocking transmit
    int state = radio.transmit(txBuffer, 4);
    
    // 2. CRITICAL: Clear the flag immediately after transmit.
    // The transmit hardware just fired an interrupt saying "I'm done sending".
    // We reset our flag here so we don't mistake that for a "Received packet" signal.
    delay(2);
    receivedFlag = false; 

    if (state == RADIOLIB_ERR_NONE) {
      probe->radio.isTransactionActive = true;
      probe->radio.state = RADIOLIB_ERR_NONE;
      startTime = millis();
      
      // 3. Start listening for the response
      radio.startReceive(); 
      return 0; 
    }
    
    probe->radio.state = state;
    return -1;
  } else {
    // --- WAITING PHASE ---
    
    // Use our global volatile flag which is updated by the setFlag ISR
    if (receivedFlag) {
      probe->radio.isTransactionActive = false;
      receivedFlag = false; // Reset for next time

      uint8_t byteArr[4];
      // readData is public and safe
      int state = radio.readData(byteArr, 4);
      // Serial.printf("Received byte array: 0x%04X 0x%04X 0x%04X 0x%04X\n", byteArr[0], byteArr[1], byteArr[2], byteArr[3]);

      probe->radio.state = state;
      probe->radio.RSSI = radio.getRSSI();
      probe->radio.SNR = radio.getSNR();
      probe->radio.frequencyError = radio.getFrequencyError();
      
      if (state == RADIOLIB_ERR_NONE) {
        if (byteArr[0] == cmd->opcode) {
          // Success: Reconstruct the 32-bit integer
          
          return ((uint32_t)byteArr[0] << 24) | ((uint32_t)byteArr[1] << 16) | 
          ((uint32_t)byteArr[2] << 8)  | (uint32_t)byteArr[3];
        } 
        else {
          probe->radio.state = ERR_OPCODE_MISMATCH;
        }
      }
      return -1; 
    }

    // 4. Handle actual timeout
    if (millis() - startTime > RESPONSE_TIMEOUT_MS) {
      probe->radio.isTransactionActive = false;
      probe->radio.state = TIMEOUT_ERROR_CODE;
      
      // Put radio in standby to stop the failed receive attempt
      radio.standby(); 
      return -1;
    }
    
    return 0; // Still waiting
  }
}


void printRadioStatus(Radio* radioStatus) {
  if (radioStatus->state == RADIOLIB_ERR_NONE) {
    Serial.printf("[RADIO] OK | RSSI: %.2f dBm | SNR: %.2f dB | Frequency Error: %.2f Hz\n", 
      radioStatus->RSSI, radioStatus->SNR, radioStatus->frequencyError);
  } else if (radioStatus->state == TIMEOUT_ERROR_CODE) {
    Serial.println(F("[RADIO] TIMEOUT - Probe not responding"));
  } else if (radioStatus->state == ERR_OPCODE_MISMATCH) {
    Serial.println(F("[RADIO] ERROR - Opcode mismatch"));
  } else if (radioStatus->state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println(F("[RADIO] CRC ERROR - Corrupted packet"));
  } else {
    Serial.printf("[RADIO] FAIL - Code: %d\n", radioStatus->state);
  }
}
  

void parseCommand(uint32_t value, Command* cmd) {
  cmd->opcode = (value >> 24) & 0xFF;
  cmd->params[0] = (value >> 16) & 0xFF;
  cmd->params[1] = (value >> 8) & 0xFF;
  cmd->params[2] = value & 0xFF;
}


void process_sensor_data(Probe* probe, Command* cmd) {   
    // Data is in bytes 1 and 2 (params[0] and [1])
    uint16_t data16 = (cmd->params[1] << 8) | cmd->params[2]; 
    
    switch (cmd->opcode) {
        case BATTERY_READ_OPCODE:
            probe->sensorData.batteryVoltage = (1/k_VBAT) * (float)data16 / 4095.0;            
            Serial.printf("[DATA] Battery: %.2fV\n", probe->sensorData.batteryVoltage);
            break;

        case HMC_READ_OPCODE: {
            int axis = cmd->params[0]; // Which axis did the FPGA say this is?
            int16_t val = (int16_t)data16;
            if (axis == 1)      probe->sensorData.mag.axis1 = val;
            else if (axis == 2) probe->sensorData.mag.axis2 = val;
            else if (axis == 3) probe->sensorData.mag.axis3 = val;
            
            // Serial.printf("[DATA] Magnetometer Axis %d: %d\n", axis, val);
            
            break;
        }

        case ACCEL_READ_OPCODE: {
            int axis = cmd->params[0]; // Which axis did the FPGA say this is?
            int16_t val = (int16_t)data16;
            if (axis == 1)      probe->sensorData.accel.x = val;
            else if (axis == 2) probe->sensorData.accel.y = val;
            else if (axis == 3) probe->sensorData.accel.z = val;
            
            Serial.printf("[DATA] Accelerometer Axis %d: %d\n", axis, val);
            break;
        }

        case PD_READ_OPCODE: {
            int channel = cmd->params[0]; // Which photodiode channel did the FPGA say this is?
            uint16_t val = data16;
            if (channel == 1)      probe->sensorData.photodiodes.PD[0] = val;
            else if (channel == 2) probe->sensorData.photodiodes.PD[1] = val;
            else if (channel == 3) probe->sensorData.photodiodes.PD[2] = val;
            else if (channel == 4) probe->sensorData.photodiodes.PD[3] = val;
            
            Serial.printf("[DATA] Photodiode Channel %d: %d\n", channel, val);
            break;
        }
        
    }
}


void buildCommand(Command* cmd, CommandType type) {
  // Initialize command with zeros
  cmd->opcode = 0;
  memset(cmd->params, 0, sizeof(cmd->params));

  switch (type)
  {
  case GET_BAT_VOLTAGE:
    cmd->opcode = BATTERY_READ_OPCODE;
    break;
  case GET_HMC_AXIS_1:
  case GET_HMC_AXIS_2:
  case GET_HMC_AXIS_3:
    cmd->opcode = HMC_READ_OPCODE;
    // Map Axis 1, 2, 3 to Param[2]
    if (type == GET_HMC_AXIS_1) cmd->params[2] = 1;
    if (type == GET_HMC_AXIS_2) cmd->params[2] = 2;
    if (type == GET_HMC_AXIS_3) cmd->params[2] = 3;
    break;
  case GET_ACCEL_X:
  case GET_ACCEL_Y:
  case GET_ACCEL_Z:
    cmd->opcode = ACCEL_READ_OPCODE;
    // Map X, Y, Z to Param[2]
    if (type == GET_ACCEL_X) cmd->params[2] = 1;
    if (type == GET_ACCEL_Y) cmd->params[2] = 2;
    if (type == GET_ACCEL_Z) cmd->params[2] = 3;
    break;
  case GET_PD1:
  case GET_PD2:
  case GET_PD3:
  case GET_PD4:
    cmd->opcode = PD_READ_OPCODE;
    if (type == GET_PD1) cmd->params[2] = 1;
    if (type == GET_PD2) cmd->params[2] = 2;
    if (type == GET_PD3) cmd->params[2] = 3;
    if (type == GET_PD4) cmd->params[2] = 4;
    break;
  default:
    Serial.println(F("[FATAL] Invalid CommandType in buildCommand()"));    
    break;
  }
}


