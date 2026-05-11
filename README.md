# AMR_probe_soc
SOC sources for full AMR system including 32-bit RISC-V, HDL driver for peripherals and C source code

# Change absolute paths in project folder
- HDL-> risc_v -> bootloader_rom.sv: parameter BOOTROM_FILE = "C:xxx"
- test -> tb_risc_v -> testbench.sv: string test_root = "C:xxx"