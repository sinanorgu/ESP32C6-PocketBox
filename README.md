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
- Settings application with Wi-Fi network scanning and an on-screen SSID list.
- Keyboard Test application that displays text received through the event system.
- Shell application prototype with an on-screen prompt and text entry.
- Mock, Reader, Camera, and Music application icons for UI and launcher development.

### Event and input system

- Fixed-size FreeRTOS event queue with mutex-protected push/pop operations and a capacity of 32 events.
- Event definitions for buttons, keyboard, mouse, application lifecycle, battery, Wi-Fi, and Bluetooth state.
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
  - `cat <file>` — print a file.
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

Inside the currently implemented applications, **Left** returns to the launcher. Button mapping is application-specific and is expected to be unified as the event system matures.

## Connecting over SSH

After the device joins Wi-Fi, read its IP address from the serial monitor and connect with the username configured in `src/main.cpp`:

```bash
ssh <username>@<device-ip>
```

The SSH host key is generated on first use and stored at `/PocketBox/System/ssh_host_ed25519_key` on the SD card. Only password authentication is currently supported.

## Current limitations / not supported yet

- Wi-Fi SSID/password and SSH username/password are hard-coded in `src/main.cpp`; there is no provisioning flow or secure credential storage.
- The initial SD configuration stores a username and password as plain-text JSON.
- Wi-Fi state is not fully synchronized with the system status model, and reconnect/disconnect handling is not implemented.
- Settings can scan and display SSIDs, but cannot select a network, enter a password, save it, or connect from the UI.
- Display Settings and System Info entries are placeholders.
- The on-device Shell application accepts text but does not execute the entered command or render command output yet.
- The SSH banner mentions `help`, but a `help` command is not implemented.
- `reboot` and `shutdown` produce shell result values, but the SSH server does not perform either action.
- SSH supports one client at a time, password authentication only, and no command history; Up/Down escape sequences are placeholders.
- BLE uses a custom GATT characteristic and is not a standard Bluetooth HID keyboard service.
- BLE input expects a complete 32-bit code point and does not validate characteristic payload length or provide pairing/bonding controls.
- Button input is polled directly with blocking debounce delays; button events and repeat behavior are defined but not wired into the event queue.
- Keyboard key-up/key-down, mouse, application lifecycle, battery, Wi-Fi, and Bluetooth event types are defined but not fully produced or consumed.
- Application folders exist in the model but are not shown or navigable in the launcher. Each demo currently creates a separate `Utilities` folder instance.
- Reader, Camera, Music, and Mock App are visual placeholders and do not provide their named functionality.
- The status-panel clock is hard-coded to `12:34`; there is no RTC or NTP time synchronization.
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

- [ ] Replace direct button polling with debounced button events.
- [ ] Implement press, release, long-press, and repeat semantics.
- [ ] Route Wi-Fi, BLE, SD-card, application, and power state changes through the event queue.
- [ ] Add event subscriptions/dispatch so applications do not busy-wait on the global queue.
- [ ] Audit queue locking and lifecycle behavior, then add overflow diagnostics.


### User interface

- [ ] Make application folders visible and navigable; consolidate the duplicate `Utilities` folders.
- [ ] Implement a real clock using NTP, with timezone configuration and offline fallback.
- [ ] Complete Wi-Fi, display, and system-information settings pages.
- [ ] Add reusable dialogs, notifications, an on-screen keyboard, and error screens.
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
