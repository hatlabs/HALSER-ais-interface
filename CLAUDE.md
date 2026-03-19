# CLAUDE.md

## Project Overview

HALSER AIS interface firmware — an ESP32-C3 firmware that bridges a Matsutec HA-102 AIS transponder to NMEA 2000 and Signal K networks via the HALSER board.

## Build Commands

```bash
# Build firmware
pio run

# Upload to connected board
pio run -t upload

# Monitor serial output
pio device monitor
```

## Architecture

### Data Flow

```
Matsutec HA-102 (NMEA 0183, 38400 bit/s, GPIO 3 RX / GPIO 2 TX)
  → NMEA0183IOTask (dedicated FreeRTOS task)
  → Matsutec parsers (MMSI, Static Ship Data, Voyage Data)
  → TCP server (port 10110) for raw NMEA 0183 relay
  → TCP client for Signal K server relay
  → tNMEA2000_esp32 (TWAI, GPIO 4 TX / GPIO 5 RX)
  → SSD1306 OLED display (I2C, GPIO 6 SDA / GPIO 7 SCL)
```

### Source Layout

- `src/main.cpp` — Application entry point, wiring
- `src/sender/n2k_senders.h` — N2K sender base class
- `src/ssd1306_display.h/.cpp` — OLED display (hostname, IP, uptime)
- `src/matsutec_ha102_parser.h` — Matsutec proprietary sentence parsers (MMSI, ship data, voyage data)
- `src/matsutec_config.h` — Matsutec HA-102 configuration (MMSI, ship data, voyage data)
- `src/streaming_tcp_server.h` — TCP server for raw NMEA 0183 relay
- `src/streaming_tcp_client.h/.cpp` — TCP client for Signal K relay
- `src/buffered_tcp_client.h` — TCP client with RX line buffer
- `src/ui_config.h` — Port and host+port configuration UI controls

### Hardware Pin Assignments

| Pin | Function |
|-----|----------|
| GPIO 2 | UART1 TX (to Matsutec) |
| GPIO 3 | UART1 RX (from Matsutec) |
| GPIO 4 | CAN TX |
| GPIO 5 | CAN RX |
| GPIO 6 | I2C SDA |
| GPIO 7 | I2C SCL |
| GPIO 8 | RGB LED (SK6805) |
| GPIO 9 | Button |

### Matsutec HA-102 Configuration

The transponder is configured via proprietary NMEA 0183 sentences:
- `$PAMC,Q,MID` / `$PAMC,C,MID` — Query/set MMSI
- `$ECAIQ,SSD` / `$AISSD` — Query/set static ship data
- `$ECAIQ,VSD` / `$AIVSD` — Query/set voyage static data

### TCP Server/Client

- TCP server on port 10110 relays raw NMEA 0183 sentences to connected clients
- TCP client connects to a Signal K server for NMEA 0183 data relay
- Both are configurable (enable/disable, host, port) via web UI

## Dependencies

- SensESP 3.2.0 — IoT framework (WiFi, web UI, Signal K)
- SensESP/NMEA0183 — NMEA 0183 sentence parsing
- NMEA2000-library — NMEA 2000 message handling
- NMEA2000_twai — ESP32 TWAI (CAN) driver
- Adafruit NeoPixel — RGB LED control
- Adafruit SSD1306 — OLED display
- elapsedMillis — Timing utilities
