# HALSER AIS Interface

ESP32-C3 firmware for the [HALSER](https://shop.hatlabs.fi/products/halser) board that bridges a **Matsutec HA-102** AIS transponder to NMEA 2000 and Signal K networks.

## Features

- Receives AIS data via NMEA 0183 at 38400 bit/s
- Configures Matsutec HA-102 transponder via web UI:
  - MMSI
  - Static ship data (name, callsign, antenna distances)
  - Voyage data (ship type, draught, destination, etc.)
- TCP server (port 10110) for raw NMEA 0183 sentence relay
- TCP client for relaying AIS data to a Signal K server
- OLED display showing hostname, IP, and uptime
- RGB LED activity indicator
- NMEA 2000 network connectivity
- OTA firmware updates
- NMEA 2000 watchdog (optional)

## Hardware Required

- [HALSER](https://shop.hatlabs.fi/products/halser) board
- [Matsutec HA-102](http://www.matsutec.com.cn/) AIS transponder
- NMEA 2000 network connection
- Optional: SSD1306 128x64 OLED display (I2C)

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
# Build
pio run

# Upload
pio run -t upload

# Monitor serial output
pio device monitor
```

## License

See [LICENSE](LICENSE) for details.
