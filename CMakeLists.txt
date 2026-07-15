cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv)

# The compiler's bin/ must be on PATH. Once riscv-none-elf-gcc is found, CMake
# auto-locates objcopy/objdump/ar/ranlib via the riscv-none-elf- prefix.
find_program(CMAKE_C_COMPILER   riscv-none-elf-gcc REQUIRED)
find_program(CMAKE_ASM_COMPILER riscv-none-elf-gcc REQUIRED)

project(ch32v003_hid_bootloader C ASM)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Emit all build artifacts into <source>/bin/.
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

add_executable(${PROJECT_NAME}
        src/bootloader.c
    rv003usb/rv003usb.S
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/rv003usb
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CH32V003=1
    USE_TINY_BOOT=1
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -march=rv32ec_zicsr
    -mabi=ilp32e
    -Oz
    -ffunction-sections
    -fdata-sections
    -fno-builtin
    -fno-stack-protector
    -fno-jump-tables
    -flto
    -mno-save-restore
    -Wall
    -Wextra
)

target_link_options(${PROJECT_NAME} PRIVATE
    -march=rv32ec_zicsr
    -mabi=ilp32e
    -nostartfiles
    -nostdlib  # guardrail (size budget): fail the link if any libc/libgcc helper is referenced
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,--script=${CMAKE_CURRENT_LIST_DIR}/linker.ld
    -Wl,-Map=$<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.map
    -flto
    -mno-save-restore
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "${PROJECT_NAME}.elf"
    SUFFIX ""
)

# The BOOT region length in linker.ld (1920) is the size limit — ld fails the
# link ("region BOOT overflowed") if the image won't fit, so no separate check.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.bin
    VERBATIM
)
