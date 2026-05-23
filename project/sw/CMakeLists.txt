cmake_minimum_required(VERSION 3.15)
project(riscv_firmware C ASM)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_ASM_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_OBJCOPY riscv64-unknown-elf-objcopy)

set(MARCH rv32i)
set(MABI ilp32)

file(GLOB_RECURSE SRC_FILES
    src/*.c
)

file(GLOB_RECURSE ASM_FILES
    startup/*.s
    startup/*.S
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=rv32i -mabi=ilp32")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -march=rv32i -mabi=ilp32")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -march=rv32i -mabi=ilp32")

add_executable(firmware.elf
    ${SRC_FILES}
    ${ASM_FILES}
)

target_compile_options(firmware.elf PRIVATE
    -march=${MARCH}
    -mabi=${MABI}
    -ffreestanding
    -nostdlib
    -nostartfiles
    -static
    -O2
    -g
    -msmall-data-limit=0
)

target_link_options(firmware.elf PRIVATE
    -T ${CMAKE_SOURCE_DIR}/utils/linker.ld
    -Wl,-e,_start
    -Wl,-Map=${CMAKE_BINARY_DIR}/firmware.map
    -nostdlib
    -nostartfiles
    -static
)

# Tooling
set(CMAKE_OBJDUMP riscv64-unknown-elf-objdump)
set(CMAKE_READELF riscv64-unknown-elf-readelf)
set(CMAKE_NM      riscv64-unknown-elf-nm)

# Inspection output dir
set(INSPECT_DIR ${CMAKE_BINARY_DIR}/inspect)

add_custom_command(TARGET firmware.elf POST_BUILD

    # Ensure inspect dir exists
    COMMAND ${CMAKE_COMMAND} -E make_directory ${INSPECT_DIR}

    # Generate binary
    COMMAND ${CMAKE_OBJCOPY} -O binary
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.elf
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.bin

    # Inspection outputs

    # Disassembly
    COMMAND ${CMAKE_OBJDUMP} -D
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.elf
            > ${INSPECT_DIR}/disassembly.txt

    # Disassembly with source (VERY useful)
    COMMAND ${CMAKE_OBJDUMP} -S
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.elf
            > ${INSPECT_DIR}/disassembly_source.txt

    # Section layout
    COMMAND ${CMAKE_READELF} -S
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.elf
            > ${INSPECT_DIR}/sections.txt

    # Section headers
    COMMAND ${CMAKE_OBJDUMP} -h
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.elf
            > ${INSPECT_DIR}/headers.txt

    # Symbols
    COMMAND ${CMAKE_NM} -S
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.elf
            > ${INSPECT_DIR}/symbols.txt

    # Symbols sorted by size
    COMMAND ${CMAKE_NM} -S --size-sort
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.elf
            > ${INSPECT_DIR}/symbols_sorted.txt

    # ELF dump
    COMMAND ${CMAKE_OBJDUMP} -s
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.elf
            > ${INSPECT_DIR}/elf_dump.txt

    # Size info
    COMMAND riscv64-unknown-elf-size
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.elf
            > ${INSPECT_DIR}/size.txt

    # Binary hex dump
    # COMMAND hexdump -v -e '"%08_ax: " 4/1 "%02x " "\\n"'
    #         ${CMAKE_CURRENT_BINARY_DIR}/firmware.bin
    #         > ${INSPECT_DIR}/bin_hex.txt

    COMMENT "Generating binary + inspection outputs"
)