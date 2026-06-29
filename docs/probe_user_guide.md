# Probe user guide

This guide covers the probe side of the AMR Probe SoC project. The probe runs the RISC-V firmware on the FPGA, reads sensors, and sends telemetry over UART.

## What you should see

After the firmware is running, the probe prints plain CSV text to the serial terminal. The output is meant for logging and analysis, not for a human-friendly display.

The main log line looks like this:

BAT,H1,H2,H3,A1,A2,A3,P1,P2,P3,P4,dac_off,timer

Meaning of the fields:
- BAT: battery reading from the ADC
- H1/H2/H3: magnetometer values
- A1/A2/A3: accelerometer values
- P1/P2/P3/P4: photodiode readings
- dac_off: 0 when the DAC is active, 1 when it is disabled
- timer: the internal timer value printed by the firmware

The probe also reports startup messages such as SX1262 online and accelerometer online.

## Hardware setup

1. Connect the battery pack and the JST cable to the probe board.
2. Connect the FPGA board to the PC over USB.
3. Make sure the board is powered before uploading firmware.

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

## Button behavior

The onboard button toggles the DAC state. The firmware reports this in the `dac_off` field.
