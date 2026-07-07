# CH32V003 HID bootloader

**[Flash it from your browser →](https://xrip.github.io/ch32v003-hid-bootloader/webhid-flasher.html)**
(WebHID; Chrome, Edge, Opera. No install.)

A small USB HID bootloader for the CH32V003. The device enumerates as a vendor
HID device and is flashed from a browser over WebHID — no native host tooling
required.

CH32V003 has no hardware USB peripheral, so HID over a bit-banged low-speed
stack is the small and practical transport for a bootloader that must fit in
the chip's 1920-byte boot area.

## Building

The expected toolchain is the xPack RISC-V GCC toolchain. Put its `bin/` on your
`PATH`, then:

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

The build emits:

- `bin/ch32v003_hid_bootloader.elf`
- `bin/ch32v003_hid_bootloader.bin`

It fails if the binary exceeds the CH32V003 boot-area limit of 1920 bytes.

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
