# Ground station user guide

This guide covers the ground station firmware for the ESP32-based master unit. The ground station sends commands to the probe, receives telemetry, and prints selected data to the serial terminal.

## What the ground station does

The ground station is the operator interface. It can:
- poll the probe over LoRa
- display the current state on the OLED
- print selected datasets to the serial port as CSV

The serial output is the main logging interface for analysis.

## Hardware setup

1. Power the ground station with the battery switch or USB-C cable.
2. Connect the ground station to the PC over USB.
3. Open a serial terminal at 115200 baud.

## Upload firmware

1. Open the Arduino project in [ground_station/src/main/main.ino](../ground_station/src/main/main.ino).
2. Select the board: ESP32C3 Dev Module.
3. Install the required libraries:
   - RadioLib
   - Adafruit SSD1306
   - Adafruit GFX
4. Upload the sketch to the board.

## Serial logging menu

After startup, the ground station shows a menu in the serial terminal.

Either select a action on press the push-button to start logging.

Use these keys:
- 0: System log (SNR, RSSI, battery)
- 1: Magnetometer log
- 2: Accelerometer log
- 3: Photodiodes and azimuth log
- 4: Radio failure state log
- q: Stop logging and return to the menu

Each mode prints a CSV header when it starts. The output format is:
- System: Time(f),SNR(f),RSSI(f),Voltage(f)
- HMC: Time(f),H1(i16),H2(i16),H3(i16)
- Accelerometer: Time(f),X(g),Y(g),Z(g),Pitch(deg),Roll(deg)
- Photodiodes: Time(f),PD1(u16),PD2(u16),PD3(u16),PD4(u16),Azimuth(f)
- Failures: Time(f),RadioState(i)

## What the outputs mean

- Time is the elapsed time in seconds.
- SNR (dB) and RSSI (dBm) come from the radio link.
- Voltage is the probe battery reading. The physical voltage can be estimated as V_bat = raw_value × 0.036.
- The other values are the sensor values returned by the probe.
- For accelerometer data, convert the raw logged value to a signed integer, shift right by 4, and multiply by 0.00098 to obtain g.
- For magnetometer data, convert the raw value to a signed integer first and then apply B_lsb = 41.7191772000e-6 Gauss/LSB and GAUSS_TO_T = 1e-4 so that B_T = raw × B_lsb × GAUSS_TO_T.
- For photodiodes, use conv = 50000 / ((5/6.1) × 2^16) to obtain lx/LSB.

## OLED behavior

The OLED shows the current menu and the latest telemetry state. Use the button to move through menus if the hardware is configured that way.

## Saving logs

The ground station does not write files by itself. To keep a record, copy the serial output into a text file or use your terminal application to log the session.
