# FlashPack

Cross-platform Qt6 GUI for the official `adb` and `fastboot` tools
(Linux, Windows, macOS). GPL-3.0-or-later.

## Features (v0.1)

- Live device list (ADB / Recovery / Sideload / Fastboot) with mode detection
- Flash an image to a partition (drag & drop, partition guessed from file name)
- Partition list is read from the selected device (`getvar all`), with built-in defaults as fallback
- Searchable partition field (substring match) with partition sizes shown next to names; logical partitions are marked
- A/B devices: "Use the inactive slot" toggle (`boot` -> `boot_b`), applies to flash and erase
- Image-fit check: compares the image (sparse images are measured unsparsed) with the partition size
- Erase a partition (typed confirmation)
- Typed confirmation for critical partitions (`bootloader`, `xbl`, `modem`, ...)
- Read `getvar all` into a filterable table
- Reboot to system / bootloader / fastbootd / recovery
- Per-device task queue: one command at a time per device, devices in parallel
- Finds `adb`/`fastboot` automatically (custom folder, bundled, `PATH`, Android SDK)

## Build

Requires Qt 6.2+ and CMake 3.21+.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ./build/flashpack

Runtime dependency: Android platform-tools (`adb`, `fastboot`).
On Windows/macOS, point FlashPack at the folder via **File → Platform-tools folder…**
or place a `platform-tools` directory next to the executable.

## Architecture

    ui/        MainWindow (talks only to DeviceManager and IBackend)
    core/      Device, DeviceManager (polling, change detection)
    backend/   IBackend  ->  CliBackend (QProcess), Parsers, ToolLocator

`IBackend` keeps the door open for a native USB (libusb) backend later.

## Roadmap

- Flash profiles (`.flashpack` files: partition -> image, flags, compatibility checks)
- A/B slot switching, `--disable-verity` / `--disable-verification` toggles
- Bootloader unlock/lock with big warnings
- Auto-download of platform-tools on Windows/macOS
- Windows driver and Linux udev helpers
