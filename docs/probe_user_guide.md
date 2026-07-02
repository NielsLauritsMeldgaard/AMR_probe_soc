# Probe user guide

This guide covers the probe side of the AMR Probe SoC project. The probe runs the RISC-V firmware on the FPGA, reads sensors, and sends telemetry over UART.

## What you should see

After the firmware is running, the probe prints plain CSV text to the serial terminal. The output is meant for logging and analysis, not for a human-friendly display.

The main log line looks like this:

BAT,H1,H2,H3,A1,A2,A3,P1,P2,P3,P4,dac_on,timer

Meaning of the fields:
- BAT: battery reading from the ADC. The physical voltage can be estimated as V_bat = raw_value × 0.036.
- H1/H2/H3: magnetometer values. Convert the raw reading to a signed integer, then apply B_lsb = 41.7191772000e-6 Gauss/LSB and GAUSS_TO_T = 1e-4 so that B_T = raw × B_lsb × GAUSS_TO_T.
- A1/A2/A3: accelerometer values. These are logged as raw hexadecimal values; to get physical units, interpret them as signed integers, shift right by 4, and multiply by 0.00098 to obtain g.
- P1/P2/P3/P4: photodiode readings. Convert with conv = 50000 / ((5/6.1) × 2^16) to obtain lx/LSB.
- dac_on: 1 when the DAC is active, 0 when it is disabled
- timer: the internal timer value printed by the firmware

The probe also reports startup messages such as SX1262 online and accelerometer online.

## Hardware setup

1. Connect the battery pack and the JST cable to the probe board.
2. Connect the FPGA board to the PC over USB.
3. Make sure the board is powered before uploading firmware.
4. IMPORTANT: Always press BTN1 for 1s to enter bootloader mode before uploading firmware.

## Upload firmware
1. Build the firmware first using the instructions in [build_and_vivado_guide.md](build_and_vivado_guide.md).
2. Open [project/scripts/upload_program.bat](../project/scripts/upload_program.bat).
3. Set the COM port in the script. Example:
   - set "PORT=COM7"
4. Run the script. The firmware binary is loaded through the UART bootloader.

## Connect to the serial terminal

Use any serial terminal at 115200 baud.

Expected startup messages:
- SX1262 online. System Ready.
- Accelerometer online. System Ready.
- CSV Format: BAT,H1,H2,H3,A1,A2,A3,P1,P2,P3,P4

## Logging behavior

The probe prints a new CSV line periodically. The values are raw telemetry values, so they can be copied directly into a spreadsheet or saved to a text file.

Notes:
- HMC and accelerometer values are printed as hexadecimal values.
- Photodiode values are printed as unsigned decimal values.
- The firmware also sends responses to LoRa commands from the ground station.

## Turning feedback ON and OFF

The onboard BTN0 toggles the DAC. The firmware reports this in the `dac_on` field. This means that it's possible to quickly change between open and closed loop logging.
