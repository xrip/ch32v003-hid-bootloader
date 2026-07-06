# CH32V003 HID bootloader

Low-speed USB HID bootloader for CH32V003 using `rv003usb`.

Build:

```powershell
cmake -S . -B build-ch32v003-hid -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$PWD/toolchain-riscv-none-elf.cmake"
cmake --build build-ch32v003-hid
```

Outputs:

- `build-ch32v003-hid/ch32v003-hid/ch32v003_hid_bootloader.elf`
- `build-ch32v003-hid/ch32v003-hid/ch32v003_hid_bootloader.bin`

The build fails if the binary exceeds the CH32V003 BOOT area limit of 1916 bytes.

Host flashing is done with `webhid-flasher.html` in a Chromium-based browser. The page accepts `.bin`, `.elf`, and `.uf2`, converts them in JavaScript, then sends fixed 8-byte addressed write chunks over HID feature reports.

Size-driven limitations:

- No readback verification.
- No status/error report from the device.
- If flashing does not start after reset, the bootloader jumps to the user app after a fixed busy-loop timeout.
- Device-side address validation is intentionally minimal. The WebHID page validates CH32V003 flash ranges before sending.
