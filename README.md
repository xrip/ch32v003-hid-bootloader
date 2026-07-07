# CH32V003 HID bootloader

**[Flash it from your browser →](https://xrip.github.io/ch32v003-hid-bootloader/webhid-flasher.html)**
(WebHID; Chrome, Edge, Opera. No install.)

A small USB HID bootloader for the CH32V003. The device enumerates as a
vendor HID device (VID `0x1209`, PID `0xB003`) and is flashed from a
browser over WebHID — no native host tooling required.

CH32V003 has no hardware USB peripheral, so HID over a bit-banged low-speed
stack is the small and practical transport for a bootloader that must fit
in the chip's 1920-byte boot area.

## Building

Prerequisites: **CMake ≥ 3.20**, **Ninja**, and the **xPack RISC-V GCC**
toolchain (put its `bin/` on your `PATH`).

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

Open `webhid-flasher.html` in a Chromium-based browser. The page accepts
`.bin`, `.elf`, and `.uf2` files, converts them client-side, and sends
addressed 8-byte write chunks to the device over HID feature reports. The
device enumerates as `1209:B003` — that's the filter the WebHID page uses
to find it.

On reset the bootloader waits for a bounded number of cycles. If no
first write report arrives during that window it jumps straight to the
user application; once flashing starts it stays active and writes the
payload into flash. When all pages are written, the flasher sends a
reset command and the chip reboots into the newly-written user
application.

## Limitations

These follow from the tight size budget:

- No readback verification.
- No status or error report from the device.
- If flashing does not start after reset, the bootloader jumps to the
  user app after a fixed busy-loop timeout.
- Device-side address validation is intentionally minimal; the WebHID
  page validates CH32V003 flash ranges (`0x08000000`–`0x08004000`,
  16 KB) before sending.

## Size budget and optimizations

The bootloader fits in a **1920-byte BOOT area** (the CH32V003's boot
section, remapped to `0x00000000` on reset). The linker script enforces
this — overflow fails the link. The current binary is **1792 / 1920 B
(93 %)**.

The code is intentionally terse — branchless selects, `process_report`
stripped to essentials. Notable size wins:

- `-nostdlib` guardrail: a stray `int * int` would fail the link instead
  of silently pulling `__mulsi3` (~+36 B) into the image.
- `boot_user` dedup + `noreturn` (−36 B), `flash_wait` force-inlined
  (−28 B), direct-writes to `GPIOD->CFGLR` and `RCC->APB2PCENR` (−12 B),
  dropped descriptor `wLength` clamp (−20 B).

A full library audit confirms zero libgcc/libc symbols; the rv003usb
ISR is at its feature floor (every optional macro off, `--gc-sections`
removes nothing).

## Credits

The low-speed USB stack in `rv003usb/` is
[rv003usb](https://github.com/cnlohr/rv003usb) by Charles Lohr (cnlohr).

## License

[MIT](LICENSE.txt) — Copyright (c) 2026, xrip.