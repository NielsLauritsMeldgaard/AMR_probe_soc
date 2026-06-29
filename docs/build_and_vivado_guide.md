# Build and Vivado guide

This guide covers the software build for the probe firmware and the Vivado project setup for the FPGA design.

## 1. Build the probe firmware with WSL and CMake

### Required tools

You need:
- Windows with WSL enabled
- Python 3
- CMake
- A RISC-V GNU toolchain with riscv64-unknown-elf-gcc

### Install toolchain in WSL

On Ubuntu or Debian-based WSL distributions, run:

```bash
sudo apt update
sudo apt install -y cmake make gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf
```

If your distribution does not provide the package, install the RISC-V GNU toolchain manually and make sure riscv64-unknown-elf-gcc is available in PATH.

### Build steps

From the repository root:

```bash
cd /mnt/c/Users/amads/AMR_probe_soc/project/sw
cmake -S . -B build
cmake --build build
```

### Build outputs

The build creates:
- build/firmware.elf: ELF executable for debugging and inspection
- build/firmware.bin: raw binary used by the UART upload script
- build/firmware.map: linker map
- build/inspect/: disassembly and section reports

## 2. Upload the probe firmware

The upload script is [project/scripts/upload_program.bat](../project/scripts/upload_program.bat).

Edit the script and change the COM port:

```bat
set "PORT=COM7"
```

Then run the script. It uses [project/scripts/write_bin_via_serial.py](../project/scripts/write_bin_via_serial.py) to send the binary to the FPGA.

## 3. Create or open the Vivado project

You can open the existing project at [vivado/viv_prj_AMR_probe_soc/viv_prj_AMR_probe_soc.xpr](../vivado/viv_prj_AMR_probe_soc/viv_prj_AMR_probe_soc.xpr).

If you create a new project instead, add the following sources:
- [project/hdl/src/top/datapath.sv](../project/hdl/src/top/datapath.sv)
- [project/hdl/src](../project/hdl/src) as the RTL source tree
- [project/hdl/src/ip/xadc_wiz_0](../project/hdl/src/ip/xadc_wiz_0)
- [project/hdl/utils/constraint.xdc](../project/hdl/utils/constraint.xdc)

Set the top module to `datapath`.

## 4. Absolute paths that must be updated

The project currently contains hard-coded absolute paths in a few files. Update them to match your local machine.

### Boot ROM path

In [project/hdl/src/risc_v/bootloader_rom.sv](../project/hdl/src/risc_v/bootloader_rom.sv), update:

```systemverilog
parameter BOOTROM_FILE = "C:/Users/amads/AMR_probe_soc/project/hdl/utils/boot_rom.mem"
```

Replace it with the full path to your local copy of [project/hdl/utils/boot_rom.mem](../project/hdl/utils/boot_rom.mem).

### Testbench root path

In [project/hdl/test/tb_risc_v/testbench.sv](../project/hdl/test/tb_risc_v/testbench.sv), update:

```systemverilog
string test_root = "C:/Users/amads/AMR_probe_soc/project/hdl/test/tb_risc_v/";
```

Replace it with the full path to your local [project/hdl/test/tb_risc_v](../project/hdl/test/tb_risc_v) folder.

### Simulation script paths

In [project/scripts/run_risc_v_tb.bat](../project/scripts/run_risc_v_tb.bat), update:
- XSIM_PATH to your Vivado xsim location
- TB_ROOT to the local simulation directory under the Vivado project

### Upload script port

In [project/scripts/upload_program.bat](../project/scripts/upload_program.bat), update the COM port to the one used by the FPGA board.

## 5. Build and program the FPGA

After the sources and paths are correct:
1. Run synthesis.
2. Run implementation.
3. Generate the bitstream.
4. Program the FPGA with the generated bitstream.

The bitstream is normally written to the Vivado run output directory, for example:
- vivado/viv_prj_AMR_probe_soc/viv_prj_AMR_probe_soc.runs/impl_1/datapath.bit

This bitstream programs the FPGA hardware, while the firmware binary is loaded separately through the UART bootloader.
