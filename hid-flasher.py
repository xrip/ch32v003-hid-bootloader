#!/usr/bin/env python3
"""Verbose Windows HID flasher for the CH32V003 bootloader."""

from __future__ import annotations

import argparse
import ctypes
import datetime
import pathlib
import struct
import sys
import time
import traceback
import winreg
import zlib


USB_VENDOR_ID = 0x1209
USB_PRODUCT_ID = 0xBEEF
HID_REPORT_ID = 0

COMMAND_WRITE_FLASH = 3
COMMAND_BOOT_FIRMWARE = 4

FLASH_BASE = 0x08000000
FLASH_SIZE = 16 * 1024
FLASH_PAGE_SIZE = 64
FLASH_ERASE_SIZE = 1024
FLASH_CHUNK_SIZE = 8

UF2_BLOCK_SIZE = 512
UF2_MAGIC_START_0 = 0x0A324655
UF2_MAGIC_START_1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_NOT_MAIN_FLASH = 0x00000001

HID_DEVICE_INTERFACE_GUID = "{4d1e55b2-f16f-11cf-88cb-001111000030}"
HID_REGISTRY_PATH = (
    rf"SYSTEM\CurrentControlSet\Control\DeviceClasses\{HID_DEVICE_INTERFACE_GUID}"
)

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
FILE_SHARE_READ = 0x00000001
FILE_SHARE_WRITE = 0x00000002
OPEN_EXISTING = 3
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
HIDP_STATUS_SUCCESS = 0x00110000
HIDP_FEATURE_REPORT = 2


def log(message: str) -> None:
    timestamp = datetime.datetime.now().astimezone().isoformat(timespec="milliseconds")
    print(f"[{timestamp}] {message}", flush=True)


def hex_bytes(data: bytes | bytearray) -> str:
    return " ".join(f"{byte:02X}" for byte in data)


def check_flash_range(address: int, length: int) -> None:
    flash_end = FLASH_BASE + FLASH_SIZE
    if address < FLASH_BASE or address + length > flash_end:
        raise ValueError(
            f"range 0x{address:08X}..0x{address + length:08X} is outside "
            f"0x{FLASH_BASE:08X}..0x{flash_end:08X}"
        )


class HidAttributes(ctypes.Structure):
    _fields_ = [
        ("Size", ctypes.c_uint32),
        ("VendorID", ctypes.c_ushort),
        ("ProductID", ctypes.c_ushort),
        ("VersionNumber", ctypes.c_ushort),
    ]


class HidCaps(ctypes.Structure):
    _fields_ = [
        ("Usage", ctypes.c_ushort),
        ("UsagePage", ctypes.c_ushort),
        ("InputReportByteLength", ctypes.c_ushort),
        ("OutputReportByteLength", ctypes.c_ushort),
        ("FeatureReportByteLength", ctypes.c_ushort),
        ("Reserved", ctypes.c_ushort * 17),
        ("NumberLinkCollectionNodes", ctypes.c_ushort),
        ("NumberInputButtonCaps", ctypes.c_ushort),
        ("NumberInputValueCaps", ctypes.c_ushort),
        ("NumberInputDataIndices", ctypes.c_ushort),
        ("NumberOutputButtonCaps", ctypes.c_ushort),
        ("NumberOutputValueCaps", ctypes.c_ushort),
        ("NumberOutputDataIndices", ctypes.c_ushort),
        ("NumberFeatureButtonCaps", ctypes.c_ushort),
        ("NumberFeatureValueCaps", ctypes.c_ushort),
        ("NumberFeatureDataIndices", ctypes.c_ushort),
    ]


class WindowsHidApi:
    def __init__(self) -> None:
        self.kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self.hid = ctypes.WinDLL("hid", use_last_error=True)

        self.create_file = self.kernel32.CreateFileW
        self.create_file.argtypes = [
            ctypes.c_wchar_p,
            ctypes.c_uint32,
            ctypes.c_uint32,
            ctypes.c_void_p,
            ctypes.c_uint32,
            ctypes.c_uint32,
            ctypes.c_void_p,
        ]
        self.create_file.restype = ctypes.c_void_p

        self.close_handle = self.kernel32.CloseHandle
        self.close_handle.argtypes = [ctypes.c_void_p]
        self.close_handle.restype = ctypes.c_bool

        self.get_attributes = self.hid.HidD_GetAttributes
        self.get_attributes.argtypes = [ctypes.c_void_p, ctypes.POINTER(HidAttributes)]
        self.get_attributes.restype = ctypes.c_bool

        self.get_product_string = self.hid.HidD_GetProductString
        self.get_product_string.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32]
        self.get_product_string.restype = ctypes.c_bool

        self.get_manufacturer_string = self.hid.HidD_GetManufacturerString
        self.get_manufacturer_string.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32]
        self.get_manufacturer_string.restype = ctypes.c_bool

        self.get_serial_string = self.hid.HidD_GetSerialNumberString
        self.get_serial_string.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32]
        self.get_serial_string.restype = ctypes.c_bool

        self.get_preparsed_data = self.hid.HidD_GetPreparsedData
        self.get_preparsed_data.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_void_p),
        ]
        self.get_preparsed_data.restype = ctypes.c_bool

        self.free_preparsed_data = self.hid.HidD_FreePreparsedData
        self.free_preparsed_data.argtypes = [ctypes.c_void_p]
        self.free_preparsed_data.restype = ctypes.c_bool

        self.get_caps = self.hid.HidP_GetCaps
        self.get_caps.argtypes = [ctypes.c_void_p, ctypes.POINTER(HidCaps)]
        self.get_caps.restype = ctypes.c_long

        self.get_value_caps = self.hid.HidP_GetValueCaps
        self.get_value_caps.argtypes = [
            ctypes.c_int,
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_ushort),
            ctypes.c_void_p,
        ]
        self.get_value_caps.restype = ctypes.c_long

        self.set_feature = self.hid.HidD_SetFeature
        self.set_feature.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32]
        self.set_feature.restype = ctypes.c_bool

    def open_path(self, path: str, access: int) -> int:
        ctypes.set_last_error(0)
        return self.create_file(
            path,
            access,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            None,
            OPEN_EXISTING,
            0,
            None,
        )

    def get_string(self, function, handle: int) -> str | None:
        buffer = ctypes.create_unicode_buffer(256)
        if not function(handle, buffer, ctypes.sizeof(buffer)):
            return None
        return buffer.value


class HidDevice:
    def __init__(self, api: WindowsHidApi, path: str, handle: int) -> None:
        self.api = api
        self.path = path
        self.handle = handle
        self.attributes = HidAttributes()
        self.attributes.Size = ctypes.sizeof(HidAttributes)
        self.caps = HidCaps()
        self.feature_report_ids: list[int] = []

        if not self.api.get_attributes(self.handle, ctypes.byref(self.attributes)):
            raise ctypes.WinError(ctypes.get_last_error())
        self._read_caps()

    def _read_caps(self) -> None:
        preparsed_data = ctypes.c_void_p()
        if not self.api.get_preparsed_data(self.handle, ctypes.byref(preparsed_data)):
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            status = self.api.get_caps(preparsed_data, ctypes.byref(self.caps))
            if status != HIDP_STATUS_SUCCESS:
                raise RuntimeError(f"HidP_GetCaps failed with status 0x{status & 0xFFFFFFFF:08X}")

            value_cap_count = self.caps.NumberFeatureValueCaps
            if not value_cap_count:
                return
            value_cap_size = 72
            raw_caps = (ctypes.c_ubyte * (value_cap_size * value_cap_count))()
            returned_count = ctypes.c_ushort(value_cap_count)
            status = self.api.get_value_caps(
                HIDP_FEATURE_REPORT,
                raw_caps,
                ctypes.byref(returned_count),
                preparsed_data,
            )
            if status != HIDP_STATUS_SUCCESS:
                log(
                    "HidP_GetValueCaps failed: "
                    f"status=0x{status & 0xFFFFFFFF:08X}, requested={value_cap_count}"
                )
                return
            self.feature_report_ids = sorted(
                {raw_caps[index * value_cap_size + 2] for index in range(returned_count.value)}
            )
        finally:
            self.api.free_preparsed_data(preparsed_data)

    def log_info(self) -> None:
        product = self.api.get_string(self.api.get_product_string, self.handle)
        manufacturer = self.api.get_string(self.api.get_manufacturer_string, self.handle)
        serial = self.api.get_string(self.api.get_serial_string, self.handle)
        log(f"HID path: {self.path}")
        log(
            "HID identity: "
            f"VID=0x{self.attributes.VendorID:04X}, "
            f"PID=0x{self.attributes.ProductID:04X}, "
            f"version=0x{self.attributes.VersionNumber:04X}, "
            f"product={product!r}, manufacturer={manufacturer!r}, serial={serial!r}"
        )
        log(
            "HID caps: "
            f"usage_page=0x{self.caps.UsagePage:04X}, usage=0x{self.caps.Usage:04X}, "
            f"input_bytes={self.caps.InputReportByteLength}, "
            f"output_bytes={self.caps.OutputReportByteLength}, "
            f"feature_bytes={self.caps.FeatureReportByteLength}, "
            f"feature_value_caps={self.caps.NumberFeatureValueCaps}, "
            f"feature_report_ids={self.feature_report_ids}"
        )

    def send_flash_report(
        self,
        sequence_number: int,
        command: int,
        address: int,
        payload: bytes,
        allow_disconnect: bool = False,
    ) -> bool:
        if len(payload) > FLASH_CHUNK_SIZE:
            raise ValueError(f"payload has {len(payload)} bytes; maximum is {FLASH_CHUNK_SIZE}")

        bootloader_report = bytearray(5 + FLASH_CHUNK_SIZE)
        bootloader_report[0] = command
        struct.pack_into("<I", bootloader_report, 1, address)
        bootloader_report[5 : 5 + len(payload)] = payload

        windows_report_length = self.caps.FeatureReportByteLength
        expected_report_length = 1 + len(bootloader_report)
        if windows_report_length != expected_report_length:
            raise RuntimeError(
                f"Windows feature report length is {windows_report_length}; "
                f"expected {expected_report_length}. The HID report descriptor may be stale."
            )

        windows_report = bytearray(windows_report_length)
        windows_report[0] = HID_REPORT_ID
        windows_report[1:] = bootloader_report
        report_buffer = (ctypes.c_ubyte * windows_report_length).from_buffer(windows_report)

        log(
            f"TX #{sequence_number}: command={command}, address=0x{address:08X}, "
            f"payload=[{hex_bytes(payload)}]"
        )
        log(f"TX #{sequence_number}: bootloader_report=[{hex_bytes(bootloader_report)}]")
        log(f"TX #{sequence_number}: windows_report=[{hex_bytes(windows_report)}]")

        ctypes.set_last_error(0)
        start_time = time.perf_counter()
        result = self.api.set_feature(self.handle, report_buffer, windows_report_length)
        elapsed_ms = (time.perf_counter() - start_time) * 1000.0
        windows_error = ctypes.get_last_error()
        log(
            f"TX #{sequence_number}: HidD_SetFeature result={bool(result)}, "
            f"win_error={windows_error}, time_ms={elapsed_ms:.3f}"
        )

        if result:
            return True
        if allow_disconnect:
            log("The final command may reset USB before its control status packet; treating this as expected.")
            return False
        raise OSError(windows_error, ctypes.FormatError(windows_error))

    def close(self) -> None:
        if self.handle not in (None, INVALID_HANDLE_VALUE):
            self.api.close_handle(self.handle)
            self.handle = None

    def __enter__(self) -> "HidDevice":
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback) -> None:
        self.close()


def iter_hid_paths() -> list[str]:
    paths: list[str] = []
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, HID_REGISTRY_PATH) as registry_key:
        index = 0
        while True:
            try:
                interface_key_name = winreg.EnumKey(registry_key, index)
            except OSError:
                break
            index += 1
            lower_name = interface_key_name.lower()
            if f"vid_{USB_VENDOR_ID:04x}&pid_{USB_PRODUCT_ID:04x}" not in lower_name:
                continue
            paths.append("\\\\?\\" + interface_key_name[4:])
    return paths


def open_bootloader(wait_seconds: float) -> HidDevice:
    api = WindowsHidApi()
    deadline = time.monotonic() + wait_seconds
    attempt = 0
    last_errors: dict[str, int] = {}
    while True:
        attempt += 1
        paths = iter_hid_paths()
        log(f"HID search attempt {attempt}: registry_paths={len(paths)}")
        for path in paths:
            handle = api.open_path(path, GENERIC_READ | GENERIC_WRITE)
            if handle == INVALID_HANDLE_VALUE:
                error = ctypes.get_last_error()
                last_errors[path] = error
                log(f"HID path open failed: path={path}, win_error={error}")
                continue
            try:
                device = HidDevice(api, path, handle)
            except Exception:
                api.close_handle(handle)
                raise
            if (
                device.attributes.VendorID == USB_VENDOR_ID
                and device.attributes.ProductID == USB_PRODUCT_ID
            ):
                return device
            device.close()

        if time.monotonic() >= deadline:
            details = ", ".join(f"{path}: {error}" for path, error in last_errors.items())
            raise RuntimeError(f"1209:BEEF HID bootloader not found. Open errors: {details}")
        time.sleep(0.25)


def pages_from_ranges(ranges: list[tuple[int, bytes]]) -> list[tuple[int, bytes]]:
    pages: dict[int, bytearray] = {}
    for range_index, (address, data) in enumerate(ranges):
        check_flash_range(address, len(data))
        log(
            f"range #{range_index}: address=0x{address:08X}, bytes={len(data)}, "
            f"crc32=0x{zlib.crc32(data):08X}"
        )
        for byte_index, byte_value in enumerate(data):
            byte_address = address + byte_index
            page_address = byte_address & ~(FLASH_PAGE_SIZE - 1)
            page = pages.setdefault(page_address, bytearray([0xFF] * FLASH_PAGE_SIZE))
            page[byte_address - page_address] = byte_value

    erase_sectors = {address & ~(FLASH_ERASE_SIZE - 1) for address in pages}
    for sector_address in erase_sectors:
        for page_address in range(
            sector_address,
            sector_address + FLASH_ERASE_SIZE,
            FLASH_PAGE_SIZE,
        ):
            pages.setdefault(page_address, bytearray([0xFF] * FLASH_PAGE_SIZE))

    result = [(address, bytes(data)) for address, data in sorted(pages.items())]
    for page_index, (address, data) in enumerate(result):
        log(
            f"page #{page_index}: address=0x{address:08X}, bytes={len(data)}, "
            f"crc32=0x{zlib.crc32(data):08X}, data=[{hex_bytes(data)}]"
        )
    return result


def parse_bin(file_data: bytes, base_address: int) -> list[tuple[int, bytes]]:
    if base_address & (FLASH_PAGE_SIZE - 1):
        raise ValueError("BIN base address must be 64-byte aligned")
    log(f"BIN input: base=0x{base_address:08X}, bytes={len(file_data)}")
    return pages_from_ranges([(base_address, file_data)])


def parse_elf(file_data: bytes) -> list[tuple[int, bytes]]:
    if file_data[:4] != b"\x7FELF":
        raise ValueError("bad ELF magic")
    if len(file_data) < 52 or file_data[4] != 1 or file_data[5] != 1:
        raise ValueError("only ELF32 little-endian is supported")

    program_header_offset = struct.unpack_from("<I", file_data, 28)[0]
    program_header_size = struct.unpack_from("<H", file_data, 42)[0]
    program_header_count = struct.unpack_from("<H", file_data, 44)[0]
    log(
        f"ELF input: bytes={len(file_data)}, program_header_offset={program_header_offset}, "
        f"program_header_size={program_header_size}, program_header_count={program_header_count}"
    )

    ranges: list[tuple[int, bytes]] = []
    for index in range(program_header_count):
        header_offset = program_header_offset + index * program_header_size
        if header_offset + 32 > len(file_data):
            raise ValueError(f"ELF program header #{index} is outside the file")
        header = struct.unpack_from("<IIIIIIII", file_data, header_offset)
        segment_type, file_offset, virtual_address, physical_address, file_size = header[:5]
        log(
            f"ELF header #{index}: type={segment_type}, file_offset={file_offset}, "
            f"virtual=0x{virtual_address:08X}, physical=0x{physical_address:08X}, "
            f"file_bytes={file_size}"
        )
        if segment_type != 1 or not file_size:
            continue
        if file_offset + file_size > len(file_data):
            raise ValueError(f"ELF segment #{index} is outside the file")
        address = physical_address or virtual_address
        if address < FLASH_BASE and address < FLASH_SIZE:
            address += FLASH_BASE
        ranges.append((address, file_data[file_offset : file_offset + file_size]))

    if not ranges:
        raise ValueError("ELF has no loadable segments")
    return pages_from_ranges(ranges)


def parse_uf2(file_data: bytes) -> list[tuple[int, bytes]]:
    if len(file_data) % UF2_BLOCK_SIZE:
        raise ValueError("UF2 size must be a multiple of 512")

    ranges: list[tuple[int, bytes]] = []
    block_count = len(file_data) // UF2_BLOCK_SIZE
    log(f"UF2 input: bytes={len(file_data)}, blocks={block_count}")
    for block_index in range(block_count):
        block_offset = block_index * UF2_BLOCK_SIZE
        magic_start_0, magic_start_1 = struct.unpack_from("<II", file_data, block_offset)
        magic_end = struct.unpack_from("<I", file_data, block_offset + 508)[0]
        if (
            magic_start_0 != UF2_MAGIC_START_0
            or magic_start_1 != UF2_MAGIC_START_1
            or magic_end != UF2_MAGIC_END
        ):
            raise ValueError(f"bad UF2 magic in block #{block_index}")

        flags, address, payload_length, block_number, declared_blocks = struct.unpack_from(
            "<IIIII", file_data, block_offset + 8
        )
        log(
            f"UF2 block #{block_index}: number={block_number}/{declared_blocks}, "
            f"flags=0x{flags:08X}, address=0x{address:08X}, payload_bytes={payload_length}"
        )
        if flags & UF2_FLAG_NOT_MAIN_FLASH:
            log(f"UF2 block #{block_index}: skipped because NOT_MAIN_FLASH is set")
            continue
        if payload_length > 256:
            raise ValueError(f"UF2 block #{block_index} payload is larger than 256 bytes")
        payload_start = block_offset + 32
        ranges.append((address, file_data[payload_start : payload_start + payload_length]))

    return pages_from_ranges(ranges)


def read_pages(file_path: pathlib.Path, base_address: int) -> list[tuple[int, bytes]]:
    file_data = file_path.read_bytes()
    log(
        f"input file: path={file_path.resolve()}, bytes={len(file_data)}, "
        f"crc32=0x{zlib.crc32(file_data):08X}"
    )
    suffix = file_path.suffix.lower()
    if suffix == ".uf2":
        return parse_uf2(file_data)
    if suffix == ".elf":
        return parse_elf(file_data)
    return parse_bin(file_data, base_address)


def flash_pages(
    device: HidDevice,
    pages: list[tuple[int, bytes]],
    delay_ms: float,
    erase_delay_ms: float,
    no_reset: bool,
    limit_chunks: int | None,
    step: bool,
) -> None:
    sequence_number = 0
    written_chunks = 0
    total_chunks = len(pages) * (FLASH_PAGE_SIZE // FLASH_CHUNK_SIZE)
    log(
        f"flash plan: pages={len(pages)}, chunks={total_chunks}, "
        f"delay_ms={delay_ms}, erase_delay_ms={erase_delay_ms}, "
        f"no_reset={no_reset}, limit_chunks={limit_chunks}"
    )

    for page_index, (page_address, page_data) in enumerate(pages):
        log(f"page write start: index={page_index}, address=0x{page_address:08X}")
        for page_offset in range(0, FLASH_PAGE_SIZE, FLASH_CHUNK_SIZE):
            if limit_chunks is not None and written_chunks >= limit_chunks:
                log(f"chunk limit reached after {written_chunks} writes; reset command is not sent")
                return
            address = page_address + page_offset
            payload = page_data[page_offset : page_offset + FLASH_CHUNK_SIZE]
            if step:
                input(f"Press Enter to send chunk #{written_chunks} to 0x{address:08X}...")
            sequence_number += 1
            device.send_flash_report(sequence_number, COMMAND_WRITE_FLASH, address, payload)
            written_chunks += 1
            erases_sector = (address & (FLASH_ERASE_SIZE - 1)) == 0
            wait_ms = erase_delay_ms if erases_sector else delay_ms
            log(f"TX #{sequence_number}: wait_ms={wait_ms} ({'sector erase' if erases_sector else 'program'})")
            if wait_ms:
                time.sleep(wait_ms / 1000.0)
        log(f"page write complete: index={page_index}, address=0x{page_address:08X}")

    if no_reset:
        log("all writes are complete; reset command is disabled")
        return

    if step:
        input("Press Enter to send the boot-firmware command...")
    sequence_number += 1
    device.send_flash_report(
        sequence_number,
        COMMAND_BOOT_FIRMWARE,
        0,
        b"",
        allow_disconnect=True,
    )
    log("boot-firmware command sent")


def parse_int(value: str) -> int:
    return int(value, 0)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Verbose Windows HID flasher for the CH32V003 1209:BEEF bootloader."
    )
    parser.add_argument("file", nargs="?", type=pathlib.Path, help="BIN, ELF, or UF2 file")
    parser.add_argument("--base", type=parse_int, default=FLASH_BASE, help="BIN base address")
    parser.add_argument("--probe", action="store_true", help="show HID data and exit")
    parser.add_argument("--dry-run", action="store_true", help="parse the file without USB writes")
    parser.add_argument("--no-reset", action="store_true", help="do not send the final command")
    parser.add_argument("--delay-ms", type=float, default=20.0, help="delay after each write")
    parser.add_argument("--limit-chunks", type=int, help="stop after this many 8-byte writes")
    parser.add_argument("--step", action="store_true", help="wait for Enter before each report")
    parser.add_argument(
        "--erase-delay-ms",
        type=float,
        default=80.0,
        help="delay after the first write of each 1 KB erase sector",
    )
    parser.add_argument("--wait", type=float, default=3.0, help="seconds to wait for 1209:BEEF")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    log(
        f"tool start: python={sys.version.split()[0]}, platform={sys.platform}, "
        f"VID=0x{USB_VENDOR_ID:04X}, PID=0x{USB_PRODUCT_ID:04X}, report_id={HID_REPORT_ID}"
    )

    pages: list[tuple[int, bytes]] | None = None
    if arguments.file is not None:
        pages = read_pages(arguments.file, arguments.base)
        log(f"input parse complete: pages={len(pages)}")
    elif not arguments.probe:
        raise ValueError("a BIN, ELF, or UF2 file is required unless --probe is used")

    if arguments.dry_run:
        log("dry run complete; USB was not opened")
        return 0

    with open_bootloader(arguments.wait) as device:
        device.log_info()
        if device.feature_report_ids and device.feature_report_ids != [HID_REPORT_ID]:
            raise RuntimeError(
                f"Windows reports Feature report IDs {device.feature_report_ids}; "
                f"expected [{HID_REPORT_ID}]. Remove the stale HID device data or change firmware/host together."
            )
        if arguments.probe and pages is None:
            log("probe complete")
            return 0
        assert pages is not None
        flash_pages(
            device,
            pages,
            arguments.delay_ms,
            arguments.erase_delay_ms,
            arguments.no_reset,
            arguments.limit_chunks,
            arguments.step,
        )

    log("tool complete")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as error:
        log(f"fatal error: {type(error).__name__}: {error}")
        traceback.print_exc()
        raise SystemExit(1)
