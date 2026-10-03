# ESP32-C6 PocketBox

ESP32-C6 PocketBox is an experimental, pocket-sized application platform built around the Waveshare ESP32-C6-LCD-1.47 board. It combines a small graphical application launcher, physical-button input, SD-card storage, Wi-Fi, Bluetooth Low Energy (BLE), and an SSH-accessible shell in a single embedded device. The project is intended to grow into a useful miniature computer while remaining a practical playground for ESP32-C6 firmware, user interfaces, networking, and I/O.


## Features

### Hardware and display

- Waveshare ESP32-C6-LCD-1.47 target with an ST7789 172 x 320 LCD.
- Arduino GFX-based interface, currently rendered in landscape orientation.
- PWM-controlled LCD backlight brightness.
- Four active-low physical buttons for navigation and application control.
- LCD and SD card share the FSPI clock and MOSI lines, with separate chip-select pins.
- Native USB CDC serial logging at 115200 baud.

### Graphical interface and applications

- Icon-based application launcher with selection highlighting and horizontal scrolling.
- Reusable `Application` interface with application names, run handlers, and custom icons.
- Application folder model with support for nested folders at the data-model level.
- Status panel with Wi-Fi, BLE, and SD-card indicators.
- Scrollable reusable list-menu component.
- Fixed-capacity text-box widget with text input and backspace handling.
- Settings application with Wi-Fi network scanning, known-network management, and a Network information screen.
- Network information screen with connection status, SSID, IP address, netmask, gateway, DNS, MAC address, RSSI, and channel details.
- Keyboard Test application that displays text received through the event system.
- Shell application prototype with an on-screen prompt, text entry and the `screenshot` command.
- Screenshot capture from SSH or Left+Right, saved as lossless BMP files on SD.
- Gellery image viewer with a paginated ListMenu for `/PocketBox/Gallery`, supporting JPEG, PNG and BMP.
- Mock, Reader, Camera, and Music application icons for UI and launcher development.

### Event and input system

- Fixed-size FreeRTOS event queue with mutex-protected push/pop operations and a capacity of 32 events.
- Debounced button events with short/long presses, repeats, combinations and queued virtual input.
- Event definitions for keyboard, mouse, application lifecycle, battery, Wi-Fi, and Bluetooth state.
- BLE text input is translated into Unicode `TextInput` events.
- Queue operations include push, pop, peek, size, capacity, and clear.

### SD card and file system

- SD card access over the shared FSPI bus at 10 MHz.
- SD-card presence detection and an on-screen status indicator.
- First-boot setup that attempts to create the `/PocketBox` workspace, configuration file, and a per-user directory under `/PocketBox/Users`.
- Helpers for recursive directory listing, file-size formatting, file reading/writing, and LCD directory rendering.
- Persistent Ed25519 SSH host-key generation and loading from the SD card.

### Wi-Fi and BLE

- ESP32-C6 Wi-Fi station mode.
- Synchronous network scanning, including hidden networks.
- Scan diagnostics for RSSI, channel, and security type over serial.
- Wi-Fi connection with a 15-second timeout and connection diagnostics.
- Wi-Fi connection states for disconnected, scanning, connecting, and connected status display.
- Background auto-reconnection task that retries only the last connected network when it is marked for automatic connection.
- Mutex-protected Wi-Fi connection, scanning, and network-configuration operations.
- Known networks persisted in `networks.json`, including the `lastConnected` marker used for automatic reconnection.
- BLE GATT server advertised as `ESP32C6 PocketBox`.
- Custom read/write/notify keyboard characteristic for receiving 32-bit Unicode code points.
- BLE connection callbacks, automatic advertising restart after disconnect, and UI status updates.

### Shell and SSH server

- Embedded shell backed by the SD-card file system.
- Relative and absolute path resolution with `.` and `..` normalization.
- Available commands:
  - `ls` — list the current directory.
  - `cd <path>` — change directory.
  - `pwd` — print the current directory.
  - `ifconfig` — print Wi-Fi status, SSID, IP configuration, MAC address, RSSI, and channel.
  - `cat <file>` — print a file.
  - `echo <text>` — print text; use `>` to overwrite or `>>` to append to an SD file.
  - `nano <path>` — edit an SD file over SSH (`Ctrl+O` saves, `Ctrl+X` exits; 16 KiB limit).
  - `cpl <path>` — execute a ClumsyPL source file from the SD card.
  - `touch <file>` — create a file.
  - `mkdir <directory>` — create a directory.
  - `rm <file>` — remove a file.
  - `rmdir <directory>` — remove an empty directory.
  - `cp <source> <destination>` — copy a file.
  - `mv <source> <destination>` — move or rename a path.
  - `clear` — clear the terminal.
  - `exit` — close the shell session.
  - `reboot` and `shutdown` — return system-control requests to the caller (not acted upon yet).
- SSH server on port 22 using LibSSH-ESP32 and password authentication.
- SSH terminal line editing with insertion, backspace, left/right cursor movement, and `Ctrl+C`.
- ANSI-colored prompt rooted at `/PocketBox`.
- SSH runs in a dedicated FreeRTOS task.

## Hardware pinout

| Function | GPIO |
| --- | ---: |
| Button Up | 0 |
| Button Left | 1 |
| Button Right | 2 |
| Button Down | 3 |
| SD CS | 4 |
| SD MISO | 5 |
| Shared SPI MOSI | 6 |
| Shared SPI SCLK | 7 |
| LCD CS | 14 |
| LCD DC | 15 |
| LCD Reset | 21 |
| LCD Backlight | 22 |

## Project structure

```text
include/                 Public interfaces and reusable UI/system components
src/
  Applications/          Launcher applications
  main.cpp               Hardware initialization and main loop
  System.cpp             Application registry and graphical launcher
  Shell.cpp              Shell commands and SD path handling
  SshManager.cpp         SSH server and terminal input handling
  SSHKeyManager.cpp      Persistent SSH host-key management
  WifiManager.cpp        Wi-Fi scanning and connection handling
  BleManager.cpp         BLE GATT input and event generation
  SdCardManager.cpp      PocketBox SD workspace initialization
platformio.ini           PlatformIO environment and dependencies
```

## Building and flashing

The primary build configuration uses PlatformIO with the Arduino framework.

1. Install [PlatformIO](https://platformio.org/) or the PlatformIO IDE extension.
2. Connect the ESP32-C6 board over USB.
3. Update the temporary Wi-Fi and SSH configuration in `src/main.cpp`. Do not commit real credentials.
4. Build and upload the firmware:

```bash
pio run
pio run --target upload
pio device monitor
```

The serial monitor is configured for 115200 baud. A FAT32-formatted SD card is expected. On first boot, the firmware attempts to create the PocketBox workspace and its initial configuration.

## Controls

On the launcher screen:

- **Left / Right:** move between applications.
- **Up:** open the selected application.

Inside applications, a short **Left** press returns to the previous screen or
launcher. Actions trigger on release; menu navigation also supports hold-to-repeat.
Long presses and combinations are separate events for future shortcuts. See
[button events and automation](docs/button-events.md) for the API and timing contract.

### Gellery

Copy images into `/PocketBox/Gallery` on the SD card. Gellery creates this folder
if it is missing. **Up / Down** select a filename, **Right** opens it, and **Left**
returns to the list. Press **Left** again to close the application. Each press is
consumed before changing screens. Lists have 60 files per page, with previous/next
page entries. While viewing, **Right** toggles fullscreen (hiding the status panel),
**Up** opens the previous image and **Down** opens the next image in directory/list
order, including across pages. Navigation skips non-image files and stops at the
first/last image. Fullscreen stays enabled when changing images; **Left** returns
directly to the list and restores the status panel. `ScreenShots/` opens saved
screenshots; other subdirectories are skipped.
Long names are shortened on screen.
Unsupported files remain visible and produce an explanatory message when opened.

Supported images are baseline JPEG/JPG, non-interlaced PNG with up to 8-bit
channels (including palettes and transparency), and uncompressed 24/32-bit RGB
Windows BMP. Transparent PNG pixels are composited onto black; the unused fourth
byte in 32-bit RGB BMP is ignored. Progressive JPEG, interlaced/16-bit PNG, WebP,
GIF and compressed/paletted BMP are not supported. JPEG EXIF orientation is not
applied. JPEG and PNG use [JPEGDEC](https://github.com/bitbank2/JPEGDEC) and
[PNGdec](https://github.com/bitbank2/PNGdec).

Images are centered below the status panel (or across the whole display in
fullscreen) and reduced with nearest-neighbour
sampling, preserving their aspect ratio. Small images retain their original size.
Decoding reads blocks/rows directly from SD without a full image framebuffer.
The safety limits in `include/GalleryImage.hpp` are **2 MiB per file**, **1024 pixels
per dimension**, **1,048,576 pixels total**, and a **48 KiB free-heap reserve** before
allocating buffers/decoders. Fragmented or insufficient memory produces an error.
Decode callbacks enforce a 10-second processing deadline and allow cancellation
with Left; an individual blocking SD operation is still subject to the SD driver
timeout. Decoder buffers and menu entries are released after use.

Validation: run `pio run -e esp32c6` and `pio test -e native`. On hardware, check
landscape/portrait and odd-sized images, transparent PNG, top-down/bottom-up BMP,
over 60 files, long names, empty/missing folders, oversized/truncated/unsupported
files, and repeated open/back/exit cycles. Short-press Left while viewing/loading
to confirm it returns only to the list; press again to exit. Long presses are
separate events and do not trigger short-click actions.
Also check fullscreen toggling, status-panel restoration, previous/next across
page boundaries, the first/last image, and skipping non-image files.

## Screenshots

Run `screenshot` in an SSH shell or in the on-device Shell application. A single
remote command is also supported:

```bash
ssh <username>@<device-ip> screenshot
```

Press **Left + Right together** for the same operation. The existing button chord
handler consumes the combination, so it does not also navigate or toggle fullscreen.
The button result is logged over USB serial; the shell reports the saved path or
an error, and the one-shot SSH command exits with status 0 on success or 1 on failure.
The SSH server still accepts only one connected client at a time.

Files are saved under `/PocketBox/Gallery/ScreenShots`, created if necessary:

```text
ScreenShot_20261003_153045Z_0000000042.bmp
```

Names contain UTC date/time and a 64-bit counter persisted in ESP32 NVS before each
capture. Existing final/partial files are never reused. Failed captures may leave
counter gaps. When the clock has not synchronized, the filename includes
`unsynced_` and the current unsynchronized system date; the counter still provides
uniqueness across reboots. NTP is started asynchronously using `pool.ntp.org` and
`time.cloudflare.com`. Saving does not wait for an internet connection.

Images are 320 x 172, uncompressed 24-bit BMP (165,174 bytes), preserving the
screen's RGB565 colors exactly. BMP avoids PNG compression workspace and is already
supported by Gellery. Open `ScreenShots/` in Gellery to view them; previous/next
navigation stays within that folder. Reopen the gallery if the folder was first
created while its file list was already open.

The current LCD driver has no pixel-readback API. `ScreenCaptureDisplay` mirrors
all drawing through the normal graphics interface to an SD-backed RGB565 shadow
at `/PocketBox/System/.screen.rgb565`. It uses three eight-row caches (15 KiB) and
a 960-byte output row, instead of a 107.5 KiB full-frame RAM allocation. The shadow
is initialized to black with the LCD at boot and recreated on every boot. Do not
edit/delete this internal file while the firmware is running. It adds SD I/O to
normal drawing; display performance depends on the card.

LCD writes and shadow updates share a recursive display mutex. Capture locks
out drawing while it streams the snapshot to SD, without allocating a second
framebuffer. Each LCD SPI transaction ends before shadow SD I/O starts because
the devices share the bus. A detected shadow read/write failure disables capture
until reboot rather than silently saving stale pixels. Display drawing can still
continue. An unavailable/full SD card returns an error. Incomplete writes use a
`.part` suffix and are renamed only after completion; power loss can leave a
`.part` file which may be deleted later.

Other firmware features can call `takeScreenshot()` from `ScreenCapture.hpp` and
inspect `ScreenshotResult`. Call it from task context, not an interrupt. It may
block while drawing finishes or the SD file is written. Firmware builds validate
the integration; actual LCD appearance, shared-bus latency and capture during SSH
or image decoding still need verification on the physical board.

## Connecting over SSH

After the device joins Wi-Fi, read its IP address from the serial monitor and connect with the username configured in `src/main.cpp`:

```bash
ssh <username>@<device-ip>
```

The SSH host key is generated on first use and stored at `/PocketBox/System/ssh_host_ed25519_key` on the SD card. Only password authentication is currently supported.

Single-file transfers are also supported through SCP. Upload a local file to the
PocketBox SD-card workspace with:

```bash
scp -O ./local-file.clmsypl <username>@<device-ip>:/PocketBox/
```

Download a file from the device with:

```bash
scp -O <username>@<device-ip>:/PocketBox/remote-file.clmsypl ./
```

SCP transfers use the authenticated SSH connection and are restricted to paths
under `/PocketBox`. Use `-O` to force the legacy SCP protocol because the
current implementation does not provide the SFTP subsystem. The current
implementation supports one regular file per command; recursive directory
transfers and file metadata preservation are not implemented yet.

## Current limitations / not supported yet

- Wi-Fi SSID/password and SSH username/password are hard-coded in `src/main.cpp`; there is no provisioning flow or secure credential storage.
- The initial SD configuration stores a username and password as plain-text JSON.
- Automatic reconnection retries only the last connected network when its `autoConnect` value is true; there is no fallback rotation through other saved networks.
- Wi-Fi scanning and connection operations are serialized, but the scan is synchronous and can temporarily block the Settings UI.
- Settings can scan and display SSIDs, connect to a scanned network, manage known networks, and display current Network details; provisioning is still limited to the existing password-entry flow.
- Display Settings and System Info entries are placeholders.
- The on-device Shell application executes `screenshot`; other commands remain a UI prototype.
- The SSH banner mentions `help`, but a `help` command is not implemented.
- `reboot` and `shutdown` produce shell result values, but the SSH server does not perform either action.
- SSH supports one client at a time, password authentication only, and no command history; Up/Down escape sequences are placeholders.
- SCP supports single-file upload and download under `/PocketBox`, but not recursive directories or metadata preservation.
- BLE uses a custom GATT characteristic and is not a standard Bluetooth HID keyboard service.
- BLE input expects a complete 32-bit code point and does not validate characteristic payload length or provide pairing/bonding controls.
- Button polling runs in a dedicated task; synchronous application operations still block foreground event consumption.
- Keyboard key-up/key-down, mouse, application lifecycle, battery, Wi-Fi, and Bluetooth event types are defined but not fully produced or consumed.
- Application folders exist in the model but are not shown or navigable in the launcher. Each demo currently creates a separate `Utilities` folder instance.
- Reader, Camera, Music, and Mock App are visual placeholders and do not provide their named functionality.
- The status-panel clock is still hard-coded to `12:34`. NTP synchronizes the system clock for screenshot filenames when Wi-Fi/internet are available; there is no battery-backed RTC.
- There is no battery measurement, battery icon, power management, sleep mode, or true software shutdown.
- SD-card hot-plug detection and recovery are not implemented.
- Shell parsing does not support quoting, escaped spaces, pipes, redirection, wildcards, or recursive file operations.
- The firmware has no automated tests, continuous integration, release packaging, or documented versioning policy.
- The current firmware is tied to the board's fixed pinout and display geometry; other boards are not configured.

## Roadmap / TO-DO

The list below is intentionally task-oriented so future contributors can select a contained piece of work.

### Security and configuration

- [ ] Remove all network and login credentials from source control.
- [ ] Add first-boot Wi-Fi provisioning through the screen, BLE, or a temporary access point.
- [ ] Store secrets using ESP32 NVS with appropriate protection instead of plain-text JSON.
- [ ] Add BLE pairing, bonding, authorization, and payload validation.

### Input and event system

- [x] Replace direct button polling with debounced button events.
- [x] Implement press, release, long-press, and repeat semantics.
- [ ] Route Wi-Fi, BLE, SD-card, application, and power state changes through the event queue.
- [ ] Add event subscriptions/dispatch so applications do not busy-wait on the global queue.
- [ ] Audit queue locking and lifecycle behavior, then add overflow diagnostics.


### User interface

- [ ] Make application folders visible and navigable; consolidate the duplicate `Utilities` folders.
- [ ] Implement a real clock using NTP, with timezone configuration and offline fallback.
- [ ] Complete Wi-Fi, display, and system-information settings pages.
- [ ] Add reusable dialogs, notifications, an on-screen keyboard, and error screens.
- [x] Add screenshot capture support for the device display.
- [x] Add an image viewer application with support for displaying image files.
- [ ] Optimize redraws using dirty regions and remove blocking UI loops/delays.
- [ ] Add themes, configurable brightness, and persistent display preferences.

### Shell and remote access

- [ ] Connect the on-device terminal UI to `Shell::executeCommand` with a display-backed `ShellOutput`.
- [ ] Implement `help` and add usage/error validation for every command.
- [ ] Handle `reboot` safely and define realistic shutdown/deep-sleep behavior.
- [ ] Add SSH command history and Up/Down navigation.
- [ ] Support multiple isolated SSH sessions or explicitly enforce and report the single-session limit.
- [ ] Improve the parser with quoted arguments and escaped spaces.
- [ ] Add useful commands such as `df`, `free`, `uptime`, `date`, `wifi`, and `reboot` status feedback.

### Storage and applications

- [ ] Make SD workspace creation transactional and create all required parent directories explicitly.
- [ ] Add SD-card hot-plug detection, mount state handling, and corruption/error reporting.
- [ ] Build a navigable file-manager application using the existing directory-rendering helpers.
- [ ] Turn Reader into a text-file viewer with scrolling and encoding handling.
- [ ] Replace or remove the Camera and Music placeholders based on supported hardware.
- [ ] Define an application manifest or registration convention for adding new apps.
- [ ] Design and implement Clumsy Store for repository-based Clumsy Apps. A Clumsy App will be an executable ClumsyPL project with metadata such as version, application name, icon, and permissions. Apps can be developed and updated in the repository without changing the device firmware or consuming additional flash space for built-in applications; the final package and update model is still to be designed.

### Power, reliability, and development

- [ ] Add battery voltage/charge monitoring and low-battery events.
- [ ] Add idle dimming, light sleep/deep sleep, wake sources, and power-state UI.
- [ ] Split board-specific pins and display settings into selectable hardware profiles.
- [ ] Measure heap/stack usage, especially for BLE and the SSH task, and document safe limits.
- [ ] Improve error propagation and replace temporary serial debug output with structured logging.

## Contributing

Contributions are welcome. Pick an unchecked roadmap item, keep changes focused, and describe any required hardware in the pull request. For changes that affect hardware or networking, include the serial output and a short manual test procedure. Please avoid committing personal credentials, generated host keys, or SD-card contents.
# ClumsyPL on PocketBox

The ClumsyPL interpreter is embedded under `include/clumsyPL` and
`src/clumsyPL`. It executes the same source language from either a string or an
SD card file.

```cpp
#include <SD.h>
#include "clumsyPL/ClumsyPL.hpp"

clumsy::Runtime clumsyRuntime;

void runExamples() {
    clumsyRuntime.useSerialOutput();

    clumsy::Result fromString = clumsyRuntime.execute("print(1 + 2);");
    clumsy::Result fromSd = clumsyRuntime.executeFile(SD, "/PocketBox/demo.clmsypl");

    if (!fromSd.ok()) {
        Serial.printf("ClumsyPL %s at line %d: %s\n",
            clumsy::statusText(fromSd.status), fromSd.line, fromSd.message);
    }
}
```

Device-specific functions are registered with `Runtime::addNative`. A native
definition includes its semantic signature and runtime callback, so functions
such as a future `draw(...)` are type-checked exactly like built-ins before
execution.

Run the host-side integration tests with `pio test -e native`.

## Native code coverage

Run `python3 scripts/coverage.py` to compile and execute the current native Unity
suites with Clang instrumentation and generate an LLVM coverage report. This needs
Python 3.9+, PlatformIO, Clang, `llvm-profdata` and `llvm-cov`; on macOS the LLVM
tools are resolved through `xcrun`. Use `--pio /path/to/pio` if necessary.
The separate `coverage` environment inherits the native source filter and leaves
the ESP32 firmware build unchanged.

Each run writes to a fresh `.pio/coverage/run-*` directory. Its location is recorded
in `.pio/coverage/latest.txt`. Open `html/index.html` for annotated sources and
line/branch counts; `summary.txt`, `coverage.json` and `coverage.lcov` are available
for terminal output and CI integration. Profiles from all suites are merged, and
test code, Unity and system headers are excluded. A failed test fails the command.

**These percentages cover only instrumented host code, not the whole firmware.**
Currently this is the ClumsyPL implementation and gallery image-policy helpers.
`scope.json` lists measured project files and files without coverage data; the
latter can include declaration-only headers. SSH, shell, actual screen rendering,
image decoders and hardware-dependent `ARDUINO` branches are not exercised by this
report. Coverage shows execution, not correctness: memory budgets and simultaneous
SSH/gallery operation still need separate tests on the real board. Instrumented
builds should not be used as the baseline for production memory measurements.

The instrumentation/reporting flow follows the
[LLVM source-based coverage documentation](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html).
