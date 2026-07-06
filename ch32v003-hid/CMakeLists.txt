set(TARGET ch32v003_hid_bootloader)
set(BOOTLOADER_SIZE_LIMIT 1916)

add_executable(${TARGET}
    bootloader.c
    ../rv003usb/rv003usb.S
)

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../rv003usb
)

target_compile_definitions(${TARGET} PRIVATE
    CH32V003=1
    USE_TINY_BOOT=1
)

target_compile_options(${TARGET} PRIVATE
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
