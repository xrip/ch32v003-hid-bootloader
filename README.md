# CH32V003 HID bootloader

This repository now contains a small experimental USB HID bootloader for the
CH32V003, built on top of `rv003usb`.

The original WCH UF2 Mass Storage bootloader sources were kept in git history
for reference and then removed from the working tree. The active project is the
CH32V003 low-speed HID bootloader.

## Layout

- `ch32v003-hid/` - bootloader firmware, linker script, CMake target, and
  browser-side WebHID flasher.
- `rv003usb/` - software low-speed USB stack used by the bootloader.
- `ch32v003-bootloader-docs-main/` - CH32V003 bootloader/reference docs kept
  for local reference.
- `toolchain-riscv-none-elf.cmake` - CMake toolchain file for the xPack
  RISC-V GCC toolchain.

## Build

The expected toolchain is installed at:

```powershell
C:\xpack-riscv-none-elf-gcc-15.2.0-1\bin
```

Configure and build:

```powershell
cmake -S . -B build-ch32v003-hid -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$PWD/toolchain-riscv-none-elf.cmake"
cmake --build build-ch32v003-hid
```

The build emits:

- `build-ch32v003-hid/ch32v003-hid/ch32v003_hid_bootloader.elf`
- `build-ch32v003-hid/ch32v003-hid/ch32v003_hid_bootloader.bin`

The bootloader is size-checked against the 1916-byte CH32V003 boot area limit.

## Flashing model

The device enumerates as a vendor HID device. The HTML flasher in
`ch32v003-hid/webhid-flasher.html` accepts `.bin`, `.elf`, and `.uf2` files in
the browser, converts them client-side, and sends fixed bootloader write reports
over WebHID.

The firmware waits for a bounded number of CPU cycles after reset. If no first
HID write report arrives during that window, it jumps to the user application.
Once flashing starts, the bootloader remains active and writes standard binary
payload bytes into flash.

## Notes

This is not a USB Mass Storage bootloader. CH32V003 has no hardware USB
peripheral, and `rv003usb` provides low-speed software USB, so HID is the small
and practical transport here.
