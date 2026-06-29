#include "telemetry.h"

// Instantiate global hardware objects
SX1262 radio = new Module(7, 1, 3, 2);
volatile bool receivedFlag = false;

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
/**
 * Interrupt callback for received radio packet.
 * Sets a flag indicating data is ready.
 */
void setFlag(void) {
  receivedFlag = true;
}

/**
 * Initialize SX1262 radio with fixed LoRa parameters.
 * Sets CRC, IQ inversion, DIO interrupt, and receive callback.
 */
void init_radio() {
    Serial.print(F("[SX1262] Initializing... "));
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

/**
 * Perform asynchronous master radio transaction (TX -> RX).
 * @param probe Pointer to probe state struct (stores radio + sensor data)
 * @param cmd Pointer to command to transmit
 * @return 0 if waiting, -1 on error, 32-bit value on successful response
 */
long radio_transaction_master_async(Probe* probe, Command* cmd) {
  static unsigned long startTime = 0;
  uint8_t txBuffer[4] = {0};

  if (!probe->radio.isTransactionActive) {
    txBuffer[0] = cmd->opcode; 
    txBuffer[1] = cmd->params[2]; 
    txBuffer[2] = cmd->params[1]; 
    txBuffer[3] = cmd->params[0];

    int state = radio.transmit(txBuffer, 4);
    
    delay(2);
    receivedFlag = false; 

    if (state == RADIOLIB_ERR_NONE) {
      probe->radio.isTransactionActive = true;
      probe->radio.state = RADIOLIB_ERR_NONE;
      startTime = millis();
      radio.startReceive(); 
      return 0; 
    }
    
    probe->radio.state = state;
    return -1;
  } else {
    if (receivedFlag) {
      probe->radio.isTransactionActive = false;
      receivedFlag = false;

      uint8_t byteArr[4];
      int state = radio.readData(byteArr, 4);

      probe->radio.state = state;
      probe->radio.RSSI = radio.getRSSI();
      probe->radio.SNR = radio.getSNR();
      probe->radio.frequencyError = radio.getFrequencyError();
      
      if (state == RADIOLIB_ERR_NONE) {
        if (byteArr[0] == cmd->opcode) {
          return ((uint32_t)byteArr[0] << 24) | ((uint32_t)byteArr[1] << 16) | 
                 ((uint32_t)byteArr[2] << 8)  | (uint32_t)byteArr[3];
        } else {
          probe->radio.state = ERR_OPCODE_MISMATCH;
        }
      }
      return -1; 
    }

    if (millis() - startTime > RESPONSE_TIMEOUT_MS) {
      probe->radio.isTransactionActive = false;
      probe->radio.state = TIMEOUT_ERROR_CODE;
      radio.standby(); 
      return -1;
    }
    
    return 0;
  }
}

/**
 * Print radio status in human-readable format.
 * @param radioStatus Pointer to radio state struct
 */
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

/**
 * Decode 32-bit command word into opcode + parameters.
 * @param value Raw received 32-bit value
 * @param cmd Output command struct
 */
void parseCommand(uint32_t value, Command* cmd) {
  cmd->opcode = (value >> 24) & 0xFF;
  cmd->params[0] = (value >> 16) & 0xFF;
  cmd->params[1] = (value >> 8) & 0xFF;
  cmd->params[2] = value & 0xFF;
}

/**
 * Convert command response into sensor data and update probe struct.
 * @param probe Pointer to probe state
 * @param cmd Decoded command response
 */
void process_sensor_data(Probe* probe, Command* cmd) {   
  uint16_t data16 = (cmd->params[1] << 8) | cmd->params[2]; 
  
  switch (cmd->opcode) {
      case BATTERY_READ_OPCODE:
          probe->sensorData.batteryVoltage = (1/k_VBAT) * (float)data16 / 4095.0;            
          break;

      case HMC_READ_OPCODE: {
          int axis = cmd->params[0];
          int16_t val = data16 - 32768;
          if (axis == 1)      probe->sensorData.mag.axis1 = val ;
          else if (axis == 2) probe->sensorData.mag.axis2 = val;
          else if (axis == 3) probe->sensorData.mag.axis3 = val;
          break;
      }

      case ACCEL_READ_OPCODE: {
          int axis = cmd->params[0];
          int16_t val = (int16_t)data16;
          float val_g = (float)(val >> 4) * 0.00098f;
          if (axis == 1)      probe->sensorData.accel.x = -val_g;
          else if (axis == 2) probe->sensorData.accel.y = val_g;
          else if (axis == 3) probe->sensorData.accel.z = -val_g;

          probe->sensorData.accel.pitch =
              atan2f(probe->sensorData.accel.x,
                     sqrtf(pow(probe->sensorData.accel.y, 2) + pow(probe->sensorData.accel.z, 2)));

          probe->sensorData.accel.roll =
              atan2f(probe->sensorData.accel.y, probe->sensorData.accel.z);
          break;
      }

      case PD_READ_OPCODE: {
          int channel = cmd->params[0];
          uint16_t val = data16;

          if (channel == 1)      probe->sensorData.photodiodes.PD[0] = val;
          else if (channel == 2) probe->sensorData.photodiodes.PD[1] = val;
          else if (channel == 3) probe->sensorData.photodiodes.PD[2] = val;
          else if (channel == 4) probe->sensorData.photodiodes.PD[3] = val;

          float p0 = probe->sensorData.photodiodes.PD[0];
          float p1 = probe->sensorData.photodiodes.PD[1];
          float p2 = probe->sensorData.photodiodes.PD[2];
          float p3 = probe->sensorData.photodiodes.PD[3];

          float s = p0 + p1 + p2 + p3;
          if (s < 1e-6) s = 1e-6;

          float x = ((p0 + p3) - (p1 + p2)) / s;
          float y = ((p0 + p1) - (p2 + p3)) / s;

          float az = atan2(x, y) * 180.0 / PI;
          if (az < 0) az += 360.0;
          if (az >= 360) az -= 360.0;

          probe->sensorData.photodiodes.azimuth = az;

          const float k = 0.4079;
          float r = sqrt(x*x + y*y);
          float el = atan2(r, k) * 180.0 / PI;

          probe->sensorData.photodiodes.elevation = el;
          break;
      }      
  }
}

/**
 * Build command packet from high-level command type.
 * @param cmd Output command struct
 * @param type Command type enum
 */
void buildCommand(Command* cmd, CommandType type) {
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
    if (type == GET_HMC_AXIS_1) cmd->params[2] = 1;
    if (type == GET_HMC_AXIS_2) cmd->params[2] = 2;
    if (type == GET_HMC_AXIS_3) cmd->params[2] = 3;
    break;
  case GET_ACCEL_X:
  case GET_ACCEL_Y:
  case GET_ACCEL_Z:
    cmd->opcode = ACCEL_READ_OPCODE;
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