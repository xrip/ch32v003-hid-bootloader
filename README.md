# CH32V003 USB HID bootloader + flasher

**[Flash it from your browser →](https://xrip.github.io/ch32v003-hid-bootloader/webhid-flasher.html)**
(WebHID; Chrome, Edge, Opera. No install.)

A small USB HID bootloader for the CH32V003. The device enumerates as a
vendor HID device (VID `0x1209`, PID `0xB003`) and is flashed from a
browser over WebHID, with no native host tooling required.

The CH32V003 has no hardware USB peripheral. The bootloader bit-bangs a
low-speed HID stack, which is small enough to live in the 1920-byte
boot area.

## Building

Prerequisites: CMake ≥ 3.20, Ninja, and the xPack RISC-V GCC toolchain
(put its `bin/` on your `PATH`).

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

The build emits:

- `bin/ch32v003_hid_bootloader.elf`
- `bin/ch32v003_hid_bootloader.bin`

It fails if the binary exceeds the CH32V003 boot-area limit of 1920 bytes
(the linker script enforces this).

## Flashing from the host

Open `webhid-flasher.html` in a Chromium-based browser (Chrome, Edge,
or Opera). The page accepts `.bin`, `.elf`, and `.uf2` files, converts
them client-side, and sends addressed 8-byte write chunks to the device
over HID feature reports. The device enumerates as `1209:B003` — that's
the filter the WebHID page uses to find it.

To flash:

1. Pick a firmware file.
2. Click **Flash**.
3. Plug in the CH32V003 (or reset it) — the page detects it and starts
   flashing automatically.

First time only: plug in the board *before* clicking Flash so your
browser can show the one-time permission prompt. After that, the three
steps above are all you need — a reset is enough to start each new
flash.

On reset the bootloader waits for a bounded number of cycles. If no
first write report arrives during that window it jumps straight to the
user application; once flashing starts it stays active and writes the
payload into flash. When all pages are written, the flasher sends a
reset command and the chip reboots into the newly-written user
application.

## Limitations

- No readback verification.
- No status or error report from the device.
- If flashing does not start after reset, the bootloader jumps to the
  user app after a fixed busy-loop timeout.
- Device-side address validation is intentionally minimal; the WebHID
  page validates CH32V003 flash ranges (`0x08000000`–`0x08004000`,
  16 KB) before sending.

## Size budget and optimizations

The bootloader fits in a **1920-byte BOOT area** on the CH32V003 (remapped
to `0x00000000` on reset). The linker script enforces this: overflow
fails the link. The current binary is **1792 / 1920 B (93 %)**.

The code is deliberately terse — branchless selects, `process_report`
cut down to what it needs. `-nostdlib` acts as a guardrail: a stray
`int * int` would fail the link instead of silently pulling `__mulsi3`
(~+36 B) into the image. Beyond that, a handful of small wins added up:
`boot_user` dedup + `noreturn` (−36 B), `flash_wait` force-inlined
(−28 B), direct-writes to `GPIOD->CFGLR` and `RCC->APB2PCENR` (−12 B
combined), dropping the descriptor `wLength` clamp (−20 B).

Library audit: zero libgcc/libc symbols. The rv003usb ISR is at its
feature floor — every optional macro is off and `--gc-sections` removes
nothing.

## Credits

The low-speed USB stack in `rv003usb/` is
[rv003usb](https://github.com/cnlohr/rv003usb) by Charles Lohr (cnlohr).

## License

[MIT](LICENSE.txt). Copyright (c) 2026, xrip.
