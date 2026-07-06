cmake_minimum_required(VERSION 3.20)

# When configured directly, default to the toolchain file shipped in this repo.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_LIST_DIR)
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE
            "${CMAKE_CURRENT_LIST_DIR}/toolchain-riscv-none-elf.cmake"
            CACHE FILEPATH "RISC-V cross-compilation toolchain file")
    endif()

    if(NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Generic")
        include("${CMAKE_TOOLCHAIN_FILE}")
    endif()

    project(ch32v003_hid_bootloader C ASM)
endif()

set(TARGET ch32v003_hid_bootloader)
set(BOOTLOADER_SIZE_LIMIT 1920)

add_executable(${TARGET}
    bootloader.c
    rv003usb/rv003usb.S
)

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/rv003usb
)

target_compile_definitions(${TARGET} PRIVATE
    CH32V003=1
    USE_TINY_BOOT=1
)

target_compile_options(${TARGET} PRIVATE
    -march=rv32ec_zicsr
    -mabi=ilp32e
    -Os
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

target_link_options(${TARGET} PRIVATE
    -march=rv32ec_zicsr
    -mabi=ilp32e
    -nostartfiles
    -Wl,--gc-sections
    -Wl,--script=${CMAKE_CURRENT_LIST_DIR}/linker.ld
    -Wl,-Map=$<TARGET_FILE_DIR:${TARGET}>/${TARGET}.map
    -flto
    -mno-save-restore
    --specs=nosys.specs
)

set_target_properties(${TARGET} PROPERTIES
    OUTPUT_NAME "${TARGET}.elf"
    SUFFIX ""
)

add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET}>
    COMMAND ${CMAKE_COMMAND}
        -D BIN=$<TARGET_FILE_DIR:${TARGET}>/${TARGET}.bin
        -D LIMIT=${BOOTLOADER_SIZE_LIMIT}
        -P ${CMAKE_CURRENT_LIST_DIR}/check_size.cmake
    VERBATIM
)
