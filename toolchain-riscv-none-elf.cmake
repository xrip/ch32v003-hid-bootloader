# toolchain-riscv-none-elf.cmake
# Cross-compilation toolchain for the ch32v003 pico_compat SDK.
# Use as: cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-riscv-none-elf.cmake
#
# Configurable:
#   RISCV_TOOLCHAIN_PATH - directory containing riscv-none-elf-gcc
#                         (defaults to search a few common locations)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv)

# ---------- Locate the cross compiler ----------
if(DEFINED ENV{RISCV_TOOLCHAIN_PATH})
    set(_toolchain_bin "$ENV{RISCV_TOOLCHAIN_PATH}/bin")
elseif(DEFINED RISCV_TOOLCHAIN_PATH)
    set(_toolchain_bin "${RISCV_TOOLCHAIN_PATH}/bin")
else()
    # Default search locations
    set(_toolchain_search_paths
        "C:/xpack-riscv-none-elf-gcc-15.2.0-1/bin"
        "/opt/xpack-riscv-none-elf-gcc/bin"
        "/usr/local/xpack-riscv-none-elf-gcc/bin"
        "/usr/bin"
    )
    set(_toolchain_bin "")
    foreach(p ${_toolchain_search_paths})
        if(EXISTS "${p}/riscv-none-elf-gcc.exe" OR EXISTS "${p}/riscv-none-elf-gcc")
            set(_toolchain_bin "${p}")
            break()
        endif()
    endforeach()
endif()

if(NOT _toolchain_bin)
    message(FATAL_ERROR
        "riscv-none-elf-gcc not found.\n"
        "Set RISCV_TOOLCHAIN_PATH (env var or -D) to the toolchain's root directory "
        "(the directory containing bin/, include/, lib/).")
endif()

# Find the actual compiler binary, accommodating both .exe (Windows) and no-suffix (Unix).
foreach(_candidate "riscv-none-elf-gcc.exe" "riscv-none-elf-gcc")
    if(EXISTS "${_toolchain_bin}/${_candidate}")
        set(_gcc_filename "${_candidate}")
        break()
    endif()
endforeach()

if(NOT _gcc_filename)
    message(FATAL_ERROR
        "riscv-none-elf-gcc not found in '${_toolchain_bin}'.\n"
        "Set RISCV_TOOLCHAIN_PATH (env var or -D) to the toolchain's root directory "
        "(the directory containing bin/, include/, lib/).")
endif()

# Derive the binary suffix from what we found, so helper tools match.
# gcc.exe -> .exe suffix; gcc -> empty suffix.
if(_gcc_filename MATCHES "\\.exe$")
    set(_exe_suffix ".exe")
else()
    set(_exe_suffix "")
endif()

message(STATUS "pico_compat: toolchain = ${_toolchain_bin}/${_gcc_filename}")

set(CMAKE_C_COMPILER "${_toolchain_bin}/${_gcc_filename}")
set(CMAKE_ASM_COMPILER "${_toolchain_bin}/${_gcc_filename}")
# Strip the .exe suffix from helper tools - CMake/CMakeDetermineCompiler doesn't
# add it automatically. The variable expansion below picks the right binary.
foreach(_tool IN ITEMS gcc-ar gcc-ranlib objcopy objdump size)
    if(EXISTS "${_toolchain_bin}/riscv-none-elf-${_tool}.exe")
        set(_tool_path "${_toolchain_bin}/riscv-none-elf-${_tool}.exe")
    else()
        set(_tool_path "${_toolchain_bin}/riscv-none-elf-${_tool}")
    endif()
endforeach()

set(CMAKE_C_COMPILER_AR     "${_toolchain_bin}/riscv-none-elf-gcc-ar${_exe_suffix}")
set(CMAKE_C_COMPILER_RANLIB "${_toolchain_bin}/riscv-none-elf-gcc-ranlib${_exe_suffix}")
set(CMAKE_OBJCOPY           "${_toolchain_bin}/riscv-none-elf-objcopy${_exe_suffix}")
set(CMAKE_OBJDUMP           "${_toolchain_bin}/riscv-none-elf-objdump${_exe_suffix}")
set(CMAKE_SIZE              "${_toolchain_bin}/riscv-none-elf-size${_exe_suffix}")

set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_SYSTEM_NAME Generic)

# ---------- Compile / link flags ----------
# Use a space-separated string (not a CMake list) so CMake doesn't merge the
# flags into a single ';'-separated token when passing to gcc.
# zicsr: required by ch32v00x_dbgmcu.c inline csrr/csrw, and matches the
# QINGKE V2 core's actual instruction set (it's not strictly rv32e).
set(_pico_compat_c_flags "-march=rv32ec_zicsr -mabi=ilp32e -Os -msave-restore \
    --specs=nano.specs \
    -ffunction-sections -fdata-sections -fno-common -Wall")

set(_pico_compat_ld_flags "-nostartfiles -Wl,--gc-sections -Wl,--print-memory-usage")

set(CMAKE_C_FLAGS_INIT "${_pico_compat_c_flags}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_pico_compat_ld_flags}")
set(CMAKE_ASM_FLAGS_INIT "${_pico_compat_c_flags}")

# Don't let CMake search the host system for libraries / headers / packages
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
