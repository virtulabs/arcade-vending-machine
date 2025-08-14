# Vending Controller


This project implements a microcontroller-based vending machine controller using the Elegoo ESP-WROOM-32 Devkit, an INA219 current sensor, and a 16-channel I2C relay board (PCAL9535A). It controls motors via relays, monitors current draw, and communicates with a PC or server over serial and MQTT. The codebase is organized for use with PlatformIO.

## Features

- 16-channel relay control for vending machine motors via I2C relay board (PCAL9535A)
- Current and voltage monitoring with INA219 for fault detection and feedback
- Serial and MQTT command handler for remote control and status reporting
- Direct WiFi and MQTT configuration in code (see `global.h`)
- Designed for the Elegoo ESP-WROOM-32 Devkit (ESP32)

## Hardware

- **Microcontroller:** Elegoo ESP-WROOM-32 Devkit (ESP32). Any ESP-WROOM-32 Devkit will work, provided you connect all pins correctly.
- **Current Sensor:** INA219 (any compatible, Adafruit library used for software)
- **Relay Board:** 16-channel I2C relay board (PCAL9535A)
- **Power Supply:** As required by your motors and controller

For a full Bill of Materials (BOM), pin mapping, and wiring/schematic diagrams, see [hardware/HARDWARE.md](hardware/HARDWARE.md).

## Software Structure

- **src/**: Main source code (motor control, command handling, diagnostics, etc.)
- **include/**: Project header files

Written in C++ using the Arduino framework. Uses the Adafruit INA219 library for current sensor communication and the PCAL9535A library for relay board control.

## PlatformIO Configuration

See `platformio.ini` for build environments. Example:

```ini
[env:elegoo_esp_wroom_32]
platform = espressif32
board = elegoo_esp_wroom_32
framework = arduino
monitor_speed = 115200
lib_deps = 
    adafruit/Adafruit INA219@^1.2.3
    knolleary/PubSubClient@^2.8
    arduino-libraries/NTPClient@^3.2.1
    tzapu/WiFiManager@^2.0.17
    bblanchon/ArduinoJson@^7.4.2
    chrissb/PCAL9535A Library@^1.2.3
```

## Usage

1. Connect the INA219 sensor to your ESP32 via I2C (default pins: SDA=21, SCL=22).
2. Connect the 16-channel I2C relay board (PCAL9535A) to a separate I2C bus (see `src/motor_control.cpp` for pin assignments and relay mappings).
3. Flash the firmware to the ESP32:
   - **Using PlatformIO in VS Code:**
     1. Open the PlatformIO sidebar (alien icon on the left).
     2. Click "Upload and Monitor" to build, upload, and automatically open the serial monitor.
   - **Or via command line:**
     - Run `pio run --target upload` to upload.
     - Run `pio device monitor` to open the serial monitor.
4. To send commands to the ESP32 from your PC:
   - If the serial monitor is not already open, click "Monitor" in the PlatformIO sidebar or run `pio device monitor`.
   - Type your command into the serial monitor input box and press Return/Enter to send it to the ESP32.
5. The serial monitor runs at 115200 baud and will display responses to commands.

## MQTT Usage

### Installing Mosquitto (MQTT) on macOS

To use MQTT from your MacBook terminal, install Mosquitto using Homebrew:

```bash
brew install mosquitto
```

You can start the broker with:

```bash
mosquitto -c <path-to-your-repo>/vending-controller/mosquitto.conf -v
```

Replace `<path-to-your-repo>` with the actual path to the `vending-controller` folder on your computer. You can find the `mosquitto.conf` file in the root of this repository.

### Publishing and Subscribing to Topics

- **Incoming topic:** `topic/hostToClient` (commands to ESP32)
- **Outgoing topic:** `topic/clientToHost` (responses/status from ESP32)

To publish a command to the ESP32:

```bash
mosquitto_pub -h 192.168.68.120 -p 1884 -t topic/hostToClient -m "disp;a1"
```

To subscribe and view responses/status from the ESP32:

```bash
mosquitto_sub -h 192.168.68.120 -p 1884 -t topic/clientToHost
```

Replace the IP address and port with your broker's details if different.

### Valid Commands

Commands follow the format: `<action>;<cell>\n`

Examples:

```text
disp;a1\n      # Dispense from cell a1
stop;\n        # Stop all motors
test;a1\n      # Test the motor at cell a1
test;a\n       # Test all motors in row a
test;\n        # Test all motors in the vending machine
rst;a1\n       # Reset the error flag of cell a1
rst;a\n        # Reset all error flags in row a
rst;\n         # Reset all error flags in the vending machine
```

Notes:
- The action can be `disp`, `stop`, `test`, or `rst`.
- The cell part is optional for some actions (e.g., `stop;`,  `test;`, `rst;` for all).
- All commands must end with a newline (`\n`).

## Running Tests

Unit tests have been removed in the current version. If you wish to add tests, see PlatformIO documentation for guidance.

## TODO

- Test and document serial command handler for Android Tablet communication
- Document wiring and command protocol
- Implement motor state data transfer back up to Android Tablet (send entire motorStateMatrix or only specific motor state?)
- Implement message headers from Android -> ESP32 and ESP32 -> Android -- ensure no repeat actions based on repeat messages (ie. repeat messages with the same header should only result in one action)

---



**Note:**
The project is now designed for the Elegoo ESP-WROOM-32 Devkit and a 16-channel I2C relay board. Pin mappings and I2C bus assignments are hardcoded in the firmware. Update the code and documentation if your hardware differs.

---

**For detailed hardware setup, schematics, and BOM, see:**
- [hardware/HARDWARE.md](hardware/HARDWARE.md)
- [hardware/main_schematic.png](hardware/main_schematic.png)
- [hardware/ina219_schematic.png](hardware/ina219_schematic.png)


## Libraries Used

- Adafruit INA219 (current sensor)
- PCAL9535A Library (I2C relay board)
- PubSubClient (MQTT)
- NTPClient (time synchronization)
- WiFiManager (WiFi configuration)
- ArduinoJson (JSON parsing)

---

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.