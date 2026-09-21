# sparkfun ESP32 Artnet to DMX Bridge

A high-performance Artnet to DMX bridge implementation for the sparkfun ESP32 DMX Shield.
It leverages the ESP32's dual-core architecture to ensure jitter-free DMX output while handling network traffic and debug logging on a separate core.

![SparkFun ESP32 DMX to LED Shield](image.jpg)

## 🚀 Key Features

* **Dual-Core Processing:**
    * **Core 0:** Handles WiFi stack, Art-Net UDP packet parsing, and Serial Debugging.
    * **Core 1:** Dedicated solely to the DMX timing and RS485 hardware communication.
* **WiFiManager:** No hardcoded WiFi credentials. The node starts its own Access Point (`Artnet-DMX-Node`) if no known network is found.
* **Web Portal:** Once connected, the WiFiManager portal stays reachable at `http://<ip>` (or `http://artnet-dmx-node.fritz.box` on a Fritz!Box) to change WiFi settings without re-flashing.
* **Continuous DMX Refresh:** The last received frame is re-sent continuously, so fixtures keep their state even if the Art-Net sender pauses or only sends on change.
* **Visual Feedback:** Onboard Status LED (GPIO 13) acts as a data heartbeat.
* **Debug Mode:** Global toggle to monitor incoming DMX frames and system health via Serial.
* **Collision Safety:** Uses FreeRTOS Semaphores to prevent data corruption between cores.

---

## Installation

1. Upload the code to your ESP32 using ArduinoIDE or Platformio
2. Connect your DMX equipment to the shield

---

## How to Use

1. **Power On:** The LED will blink during the boot sequence.
2. **WiFi Config:** If not connected, look for a WiFi network named **Artnet-DMX-Node** on your phone and configure your local SSID/Password. If no network is configured within 3 minutes the node restarts and tries again.
3. **Find the Node:** It shows up in your router as `artnet-dmx-node`. Open `http://<ip>` to reach the WiFi portal at any time.
4. **Send Data:** Point your Art-Net software (QLC+, MadMapper, etc.) to the ESP32's IP address on Universe 0.
5. **Enjoy:** The onboard LED will flicker when data is being received.

---

## Technical Details

- **DMX Channels**: Supports up to 512 channels (standard DMX universe)
- **Artnet Universe**: Configured for universe 0 (modifiable in code via `targetUniverse` variable)
- **Update Rate**: DMX frames are sent back-to-back (~35 fps), independent of the Art-Net input rate
- **Buffer Management**: A FreeRTOS mutex guards the shared frame buffer between cores
- **Error Handling**: Automatic restart if no WiFi is configured within 3 minutes
- **Hostname**: `artnet-dmx-node` (change `HOSTNAME` / `AP_NAME` in the code)

### Pin Mapping
The shield fixes these pins, there is nothing to wire up:

| Function | ESP32 Pin | Note |
| :--- | :--- | :--- |
| **DMX TX** | GPIO 17 | UART2 TX → RS485 transceiver DI (via optocoupler) |
| **DMX RX** | GPIO 16 | UART2 RX ← RS485 transceiver RO (unused, output only) |
| **DMX EN** | GPIO 21 | Transceiver direction: HIGH = send, LOW = receive. Pulled to *send* by the shield when not driven. Shared with I2C SDA – a Qwiic device on this pin would interrupt DMX output. |
| **Status LED** | GPIO 13 | Internal SparkFun LED |

## Code Structure

- `setup()`: Initializes hardware, WiFi (WiFiManager + web portal), and starts the two pinned tasks
- `artnetTask()`: Core 0 – Art-Net packet reception, web portal, status LED and debug monitor
- `onDmxFrame()`: Callback for incoming Art-Net data (runs inside `artnetTask`), copies the frame into the shared buffer
- `dmxOutputTask()`: Core 1 – continuously sends the DMX frame over RS485
- `loop()`: Unused; the default Arduino task deletes itself

## Debugging
Toggle the compile-time constant in the code to enable/disable Serial monitoring (when `false`, the debug code is not compiled in at all):
```cpp
constexpr bool debugEnabled = true; // Set to false for production use
```
When enabled, the node prints a DMX Snapshot of the first 16 channels every 5 seconds to the Serial Monitor (115200 Baud).

## Credits

This project is based on the [SparkFunDMX library](https://github.com/sparkfun/SparkFunDMX) and incorporates the [WiFiManager library](https://github.com/tzapu/WiFiManager) for WiFi configuration management, as well as the [ArtnetWifi library](https://github.com/rstephan/ArtnetWifi) for Artnet protocol handling.

Special thanks to:
- [SparkFun Electronics](https://github.com/sparkfun/SparkFunDMX) for the excellent DMX library
- [tzapu](https://github.com/tzapu/WiFiManager) for the WiFiManager implementation
- [rstephan](https://github.com/rstephan/ArtnetWifi) for the ArtnetWifi library

## License

This project is open source. Please refer to the individual library licenses for specific terms.

## Contributing

Feel free to submit issues and enhancement requests!

## Support

For technical support, please refer to the original library documentation:
- [SparkFunDMX Documentation](https://github.com/sparkfun/SparkFunDMX)
- [WiFiManager Documentation](https://github.com/tzapu/WiFiManager)
