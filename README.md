# CH32V003 HID bootloader

A small USB HID bootloader for the CH32V003. The device enumerates as a vendor
HID device and is flashed from a browser over WebHID — no native host tooling
required.

CH32V003 has no hardware USB peripheral, so HID over a bit-banged low-speed
stack is the small and practical transport for a bootloader that must fit in
the chip's 1916-byte boot area.

## Building

The expected toolchain is the xPack RISC-V GCC toolchain. Install it, or point
`RISCV_TOOLCHAIN_PATH` at its root directory, then:

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$PWD/toolchain-riscv-none-elf.cmake"
cmake --build build
```

The build emits:

- `build/ch32v003_hid_bootloader.elf`
- `build/ch32v003_hid_bootloader.bin`

It fails if the binary exceeds the CH32V003 boot-area limit of 1916 bytes.

## Flashing from the host

Open `webhid-flasher.html` in a Chromium-based browser. The page accepts `.bin`,
`.elf`, and `.uf2` files, converts them client-side, and sends fixed addressed
write chunks to the device over HID feature reports.

On reset the bootloader waits for a bounded number of cycles. If no first write
report arrives during that window it jumps straight to the user application;
once flashing starts it stays active and writes the payload into flash.

## Limitations

These follow from the tight size budget:

- No readback verification.
- No status or error report from the device.
- If flashing does not start after reset, the bootloader jumps to the user app
  after a fixed busy-loop timeout.
- Device-side address validation is intentionally minimal; the WebHID page
  validates CH32V003 flash ranges before sending.

## Credits

The low-speed USB stack in `rv003usb/` is
[rv003usb](https://github.com/cnlohr/rv003usb) by Charles Lohr (cnlohr).

## License

See `LICENSE`.
