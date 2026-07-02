# AMR Probe SoC

This repository contains the full AMR probe system: the FPGA-based probe hardware, the RISC-V firmware, the ground station firmware, and the Vivado project.

## Repository layout

- [project/hdl](project/hdl): HDL sources, RTL, IP, and testbenches
- [project/sw](project/sw): RISC-V firmware source, linker script, and CMake build
- [ground_station](ground_station): ESP32 ground station firmware
- [vivado](vivado): Vivado project files and generated build outputs
- [docs](docs): standalone user guides for the probe, ground station, and build/Vivado flow

## Start here

- [docs/probe_user_guide.md](docs/probe_user_guide.md): probe setup, logging, and upload flow
- [docs/ground_station_user_guide.md](docs/ground_station_user_guide.md): ground station setup and serial logging
- [docs/build_and_vivado_guide.md](docs/build_and_vivado_guide.md): firmware build, WSL tools, and Vivado path setup

## Serial settings

Use 115200 baud for both the probe UART output and the ground station serial terminal. We recommend PUTTY as terminal and saving the log for plotting.
