# CH32V003 USB HID bootloader + flasher

**[Flash it from your browser →](https://xrip.github.io/ch32v003-hid-bootloader/webhid-flasher.html)**
(WebHID; Chrome, Edge, Opera. No install.)

A small USB HID bootloader for the CH32V003. The device enumerates as a
vendor HID device (VID `0x1209`, PID `0xBEEF`) and is flashed from a
browser over WebHID, with no native host tooling required. Its USB
product name is `CH32V`.

The CH32V003 has no hardware USB peripheral. The bootloader bit-bangs a
low-speed HID stack, which is small enough to live in the 1920-byte
boot area. Flash data uses HID feature reports over control endpoint 0;
the required HID Interrupt IN endpoint is present but carries no data.
The board has a fixed 1.5 kΩ pull-up from D- to 3.3 V. On entry and
before starting the user app, the bootloader drives D- low for a short
USB disconnect.

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
them client-side, and sends addressed 8-byte chunks over HID feature
reports. The device enumerates as `1209:BEEF` — that's
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

For detailed Windows-side diagnostics, use the dependency-free Python
tool. It logs device identity, HID report sizes and IDs, every packet,
address, payload, result, Windows error, transfer time, and wait:

```powershell
python .\hid-flasher.py --probe
python .\hid-flasher.py firmware.bin --dry-run
python .\hid-flasher.py firmware.bin
```

The tool accepts `.bin`, `.elf`, and `.uf2`. Use `--base` for a BIN with
a non-default address, `--step` for one packet at a time, or
`--limit-chunks N --no-reset` for a controlled partial-write test.

On power-up the bootloader waits for about five seconds. If no
first write report arrives during that window it jumps straight to the
user application; once flashing starts it stays active and writes the
payload into flash. When all pages are written, the flasher sends a
reset command and the chip reboots into the newly-written user
application. If the first user-flash word is erased (`0xFFFFFFFF`), the
bootloader stays active instead of starting empty flash.

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
fails the link. The current binary is **1804 / 1920 B (94 %)**.

The code is size-focused, but its identifiers remain descriptive.
`-nostdlib` acts as a guardrail: a stray
`int * int` would fail the link instead of silently pulling `__mulsi3`
(~+36 B) into the image. The main size reductions are a shared descriptor
lookup table, a 16-byte report buffer with word copies, aligned command-header
reads, and the inlined `process_flash_report` handler. The build also
force-inlines `wait_for_flash`, writes the needed peripheral registers
directly, and keeps `boot_firmware` as one stable handoff point under LTO.

Library audit: zero libgcc/libc symbols. The rv003usb ISR is at its
feature floor — every optional macro is off and `--gc-sections` removes
nothing.

## Credits

The low-speed USB stack in `rv003usb/` is
[rv003usb](https://github.com/cnlohr/rv003usb) by Charles Lohr (cnlohr).

## License

[MIT](LICENSE.txt). Copyright (c) 2026, xrip.
