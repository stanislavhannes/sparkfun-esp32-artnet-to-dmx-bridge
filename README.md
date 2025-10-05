# sparkfun ESP32 Artnet to DMX Bridge

A high-performance Artnet to DMX bridge implementation for ESP32 using dual-core architecture for optimal real-time performance.

![SparkFun ESP32 DMX to LED Shield](https://camo.githubusercontent.com/1de4477b3306599a386357212f74173a8a2f9ec8a52b05ea6a7283ab794c4b3b/68747470733a2f2f63646e2e737061726b66756e2e636f6d2f722f3530302d3530302f6173736574732f70617274732f312f332f342f372f372f31353131302d537061726b46756e5f45535033325f5468696e675f506c75735f444d585f746f5f4c45445f536869656c642d30312e6a7067)

## Features

- **Dual-Core Architecture**: Utilizes both ESP32 cores for maximum performance
  - Core 0: Artnet reception over WiFi
  - Core 1: DMX output processing
- **WiFi Manager Integration**: Automatic WiFi configuration with captive portal
- **Thread-Safe Communication**: Semaphore-protected data sharing between cores
- **Real-Time Performance**: Optimized for professional lighting applications
- **Hardware Isolation**: Compatible with electrically isolated DMX hardware

## Hardware Requirements

- ESP32 development board
- [SparkFun ESP32 DMX to LED Shield (DEV-15110)](https://www.sparkfun.com/products/15110)
- DMX-compatible lighting equipment

## Pin Configuration

The code uses the following pin assignments for DMX communication:

- **DMX_TX_PIN**: GPIO 17 (Transmit)
- **DMX_RX_PIN**: GPIO 16 (Receive) 
- **DMX_EN_PIN**: GPIO 4 (Enable)

**Note**: This implementation does not utilize the LED pins from the SparkFun board, focusing solely on DMX output functionality.

## WiFi Configuration

The device creates a WiFi Access Point named **"Sparkfun-Artnet-DMX"** for easy configuration:

1. Connect to the AP when first powered on
2. Navigate to the captive portal (usually opens automatically)
3. Select your WiFi network and enter credentials
4. The device will connect and be ready for Artnet data

## Installation

1. Install the required Arduino libraries:
   - [SparkFunDMX](https://github.com/sparkfun/SparkFunDMX)
   - [ArtnetWifi](https://github.com/rstephan/ArtnetWifi)
   - [WiFiManager](https://github.com/tzapu/WiFiManager)

2. Upload the code to your ESP32

3. Connect your DMX equipment to the shield

## Usage

1. Power on the ESP32 with the shield
2. Connect to the "Sparkfun-Artnet-DMX" WiFi network
3. Configure your WiFi credentials through the captive portal
4. Send Artnet data to the device's IP address on universe 0
5. DMX data will be output in real-time to connected devices

## Technical Details

- **DMX Channels**: Supports up to 512 channels (standard DMX universe)
- **Artnet Universe**: Configured for universe 0 (modifiable in code)
- **Update Rate**: Real-time processing with minimal latency
- **Buffer Management**: Atomic operations ensure data integrity
- **Error Handling**: Automatic restart on WiFi connection failure

## Code Structure

- `setup()`: Initializes hardware, WiFi, and starts dual-core tasks
- `loop()`: Handles Artnet packet reception on Core 0
- `dmxOutputTask()`: Dedicated DMX output task running on Core 1
- `onDmxFrame()`: Callback function for incoming Artnet data

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
