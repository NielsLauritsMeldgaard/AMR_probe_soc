# Quick Start Guide: Probe

This guide covers the process of uploading firmware to the RISC-V CPU on the FPGA and monitoring the telemetry output.

## Prerequisites
* **Python 3.x** installed (required for the upload script).
* **PuTTY** or a similar Serial Terminal.
* **VS Code** (Optional, for automated tasks).

---

## 1. Uploading the Firmware

1. **Power:** Plug in the 18650 batteries (ensure correct polarity) and attach the battery JST cable to the main PCB.
2. **Bootloader Mode:** Connect the FPGA to your PC via USB. Press and hold the **Restart** button on the FPGA to enter bootloader mode. 
   * *Note: The onboard LED will turn off while the button is pressed, indicating it is ready for upload.*
3. **Configure COM Port:**
   * Open **Device Manager** on Windows and identify the COM port assigned to the FPGA.
   * Navigate to `/project/scripts/` in the repository.
   * Right-click `upload_program.bat` and select **Edit**.
   * Change the COM port number to match your device (e.g., `COM3`), then save and close the file.
4. **Execute Upload:**
   * Run `upload_program.bat` by double-clicking it.
   * **Alternative (VS Code):** Open the project folder in VS Code. Press `Ctrl+Shift+B` (or go to Terminal -> Run Task) and select the user-defined task **"UPLOAD TO FPGA"**.

---

## 2. Usage & Monitoring

After a successful upload, the probe begins its control loop. You can monitor the telemetry via UART.

1. **Open PuTTY:** Connect to the identified COM port.
2. **Settings:** Set the speed to **115200 baud**.
3. **Output:** The terminal will stream data in a CSV-ready format. To process the data, use the following header:

```csv
BAT, HMC1, HMC2, HMC3, ACCEL1, ACCEL2, ACCEL3, PD1, PD2, PD3, PD4, dac_off, timer
```

### Data Formats
* **HMC & Accelerometer:** 16-bit Hexadecimal (signed 2's complement).
* **Photodiodes:** 16-bit Unsigned Integers.
* **Battery (BAT):** Raw ADC value representing voltage.

# Change absolute paths in project folder
- HDL-> risc_v -> bootloader_rom.sv: parameter BOOTROM_FILE = "C:xxx"
- test -> tb_risc_v -> testbench.sv: string test_root = "C:xxx"
