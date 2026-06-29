# User manual

This guide describes how to deploy the Probe and Ground Station units.

---

## Quick Start: Probe

This process involves uploading firmware to the RISC-V CPU on the FPGA and monitoring raw telemetry.

### 1. Uploading the Firmware
1. **Power:** Plug in the 18650 batteries (ensure correct polarity) and attach the JST cable to the PCB.
2. **Bootloader Mode:** Connect the FPGA to your PC via USB. Press and hold the **Restart** button (the onboard LED will turn off).
3. **Configure COM Port:**
   * Open **Device Manager** to find the FPGA's COM port.
   * Navigate to `project/scripts/` and right-click `upload_program.bat` -> **Edit**.
   * Update the COM port number, then save and close.
4. **Execute:**
   * Double-click `upload_program.bat`.
   * **Alternative (VS Code):** Open the folder in VS Code and run the task **"UPLOAD TO FPGA"** (`Ctrl+Shift+B`).

### 2. Usage & Monitoring
Open **PuTTY** at **115200 baud**. The terminal will stream CSV-formatted data. Use this header for logging:
`BAT, HMC1, HMC2, HMC3, ACCEL1, ACCEL2, ACCEL3, PD1, PD2, PD3, PD4, dac_off, timer`

* **Format:** HMC/Accel are 16-bit Hex (signed 2's complement). Photodiodes are 16-bit unsigned integers.

---

## Quick Start: Ground Station

The Ground Station acts as the master controller, polling the probe and displaying processed data.

### 1. Powering On
* **Battery:** Flip the physical power switch on the unit to connect the internal battery.
* **USB:** Alternatively, connect a **USB-C** cable to power the unit and enable serial debugging.
* *Note: The firmware is stored in non-volatile flash and will execute automatically upon power-up.*

### 2. Monitoring & Interaction
* **OLED Display:** Use the physical push-button to cycle through different telemetry menus (Radio Status, IMU data, Sun Position, etc.).
* **Serial Terminal:** Connect via **PuTTY** (115200 baud) to view formatted system logs and debug information sent from the ESP32.

### 3. Programming & Updates
To modify the Ground Station software:
1. Open the project in the **Arduino IDE**.
2. **Board Selection:** Select `ESP32C3 Dev Module`.
3. **Dependencies:** Ensure the following libraries are installed via the Library Manager:
   * **RadioLib** (for SX1262 control)
   * **Adafruit SSD1306** & **Adafruit GFX** (for the OLED)
4. Click **Upload** to flash the new firmware via USB-C.

---

### System Requirements
* **Python 3.x** (for Probe upload scripts).
* **Arduino IDE** (for Ground Station development).
* **Serial Terminal** (PuTTY or Arduino Serial Monitor).

### Data Formats
* **HMC & Accelerometer:** 16-bit Hexadecimal (signed 2's complement).
* **Photodiodes:** 16-bit Unsigned Integers.
* **Battery (BAT):** Raw ADC value representing voltage.

# Change absolute paths in project folder
- HDL-> risc_v -> bootloader_rom.sv: parameter BOOTROM_FILE = "C:xxx"
- test -> tb_risc_v -> testbench.sv: string test_root = "C:xxx"
