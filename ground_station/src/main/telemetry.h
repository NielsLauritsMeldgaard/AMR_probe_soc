#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <RadioLib.h>

// --- Constants ---
#define INTERVAL_MS 100
#define RESPONSE_TIMEOUT_MS 5000 
#define k_VBAT 0.036
#define GAUSS_PER_LSB 41.7191772e-6f
#define TIMEOUT_ERROR_CODE 0xAAAA
#define ERR_OPCODE_MISMATCH 0xBBBB 

#define BATTERY_READ_OPCODE 0x01
#define HMC_READ_OPCODE 0x02
#define ACCEL_READ_OPCODE 0x03
#define PD_READ_OPCODE 0x04

typedef enum {
  GET_BAT_VOLTAGE,
  GET_HMC_AXIS_1,
  GET_HMC_AXIS_2,
  GET_HMC_AXIS_3,
  GET_ACCEL_X,
  GET_ACCEL_Y,
  GET_ACCEL_Z,
  GET_PD1,
  GET_PD2,
  GET_PD3,
  GET_PD4
} CommandType;


// --- Global Objects (Declared as extern) ---
extern SX1262 radio;
extern volatile bool receivedFlag;


// --- Structs ---
typedef struct {
  uint8_t opcode;
  uint8_t params[3]; // Up to 3 bytes of parameters
} Command;

typedef struct {
  float x, y, z;
  float pitch, roll;
} Accelerometer;

typedef struct {
  int16_t axis1, axis2, axis3;
  float axis1_G, axis2_G, axis3_G;
} Magnetometer;

typedef struct {
  uint16_t PD[4];
  float x_comb, y_comb, z_comb;
  float azimuth, elevation;
} Photodiodes;

typedef struct {
  float batteryVoltage;
  Photodiodes photodiodes;
  Accelerometer accel;
  Magnetometer mag;
} SensorData;

typedef struct {
  float RSSI;
  float SNR;
  float frequencyError;
  int state;
  bool isTransactionActive;
} Radio;

typedef struct {
  SensorData sensorData;
  Radio radio;
} Probe;

// --- Function Prototypes ---
void init_radio();
void setFlag();
long radio_transaction_master_async(Probe* probe, Command* cmd);
void parseCommand(uint32_t value, Command* cmd);
void printRadioStatus(Radio* radio);
void process_sensor_data(Probe* probe, Command* cmd);
void buildCommand(Command* cmd, CommandType type);
#endif