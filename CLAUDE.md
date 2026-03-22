# CLAUDE.md

## Project Overview

HALSER AIS interface firmware — an ESP32-C3 firmware that bridges a Matsutec HA-102 AIS transponder to NMEA 2000 and Signal K networks via the HALSER board. Also serves as a reference implementation for SensESP-based marine interface firmware.

## Build Commands

```bash
# Build firmware
pio run

# Upload to connected board
pio run -t upload

# Monitor serial output
pio device monitor

# Run unit tests (native platform)
pio test -e native
```

## Architecture

### Data Flow

```
Matsutec HA-102 (NMEA 0183, 38,400 bit/s, GPIO 3 RX / GPIO 2 TX)
  → NMEA0183IOTask (dedicated FreeRTOS task)
    → Matsutec config parsers (MMSI, ship data, voyage data)
    → RMC parser (GNSS time sync)
    → AIS VDM/VDO parser
      → AISReassembler (multi-part message assembly)
        → AIS Decoder (6-bit binary → typed structs)
          → N2K Senders → tNMEA2000_esp32 (TWAI, GPIO 4 TX / GPIO 5 RX)
          → SK Output → Signal K server (per-vessel context)

Web UI ←→ Config objects ←→ Matsutec (serial commands)
Signal K server → SKValueListeners → Voyage data config → Matsutec
```

### Source Layout

**AIS Decoding** (`src/ais/`) — framework-agnostic, no Arduino/SensESP dependencies:
- `ais_message_types.h` — Data structs for 6 AIS message types (Class A/B position, Class A/B static, safety, AtoN)
- `ais_decoder.h/.cpp` — Binary payload decoding (ITU 6-bit armored characters → typed structs)
- `ais_reassembler.h/.cpp` — Multi-part VDM sentence reassembly with 2-second timeout
- `ais_vdm_fields.h` — VDM sentence field extraction and decode orchestration
- `ais_vdm_parser.h` — SensESP `SentenceParser` integration; produces typed AIS message outputs for downstream consumers
- `ais_conversions.h` — Unit conversions (heading, COG, SOG, ROT, dimensions, ETA) between AIS, N2K, and SI

**Matsutec Configuration** (`src/`):
- `matsutec_config.h` — Config objects (MMSIConfig, ShipDataConfig, VoyageStaticDataConfig) that query/set transponder state via proprietary NMEA 0183 sentences
- `matsutec_ha102_parser.h` — SentenceParser subclasses for Matsutec response sentences (PAMC, AISSD, AIVSD)
- `operating_mode.h` — Framework-agnostic TX/RX mode state machine (zero MMSI disables transmission)
- `operating_mode_config.h` — SensESP Saveable/Serializable wrapper for operating mode

**NMEA 2000 Output** (`src/sender/`):
- `ais_n2k_senders.h` — ValueConsumer classes that convert AIS structs to N2K PGNs (129038, 129039, 129794, 129802, 129809, 129810, 129041)

**Signal K Output** (`src/signalk/`):
- `sk_ais_output.h` — Wires AIS parser outputs to Signal K contextual outputs (per-vessel/AtoN context by MMSI)
- `sk_contextual_output.h` — SensESP extension: SKOutput with dynamic context (input is `pair<context, value>`)

**Application** (`src/`):
- `main.cpp` — Application entry point; initializes all components and wires the data pipeline
- `ssd1306_display.h/.cpp` — OLED display driver (hostname, IP, uptime; updates every 1 second)

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

### Matsutec HA-102 Protocol

Configuration uses proprietary NMEA 0183 sentences:

**MMSI:**
- Query: `$PAMC,Q,MID*xx` → Response: `$PAMC,R,MID,123456789,000000000*xx`
- Set: `$PAMC,C,MID,123456789,000000000*xx` → Echo confirms

**Static ship data:**
- Query: `$ECAIQ,SSD*xx` → Response: `$AISSD,CALLSGN,SHIPNAME,030,010,03,05,0,*xx`
- Set: same format as response

**Voyage static data:**
- Query: `$ECAIQ,VSD*xx` → Response: `$AIVSD,36,2.2,2,DESTINATION,070000,15,06,0,0*xx`
- Set: same format as response (fields: ship_type, draught, persons, dest, HHMMSS, day, month, nav_status, flags)

**Operating mode:**
- Receive-only: set MMSI to `000000000` (disables TX, preserves config)
- Transmit+Receive: set MMSI to configured value

### AIS Message Types Supported

| Type | Description | N2K PGN |
|------|-------------|---------|
| 1, 2, 3 | Class A Position Report | 129038 |
| 5 | Class A Static and Voyage Data | 129794 |
| 14 | Safety-Related Broadcast | 129802 |
| 18, 19 | Class B Position Report | 129039 |
| 21 | Aid-to-Navigation Report | 129041 |
| 24A | Class B Static Data (name) | 129809 |
| 24B | Class B Static Data (type, callsign, dims) | 129810 |

### Signal K Paths

**Output (per-vessel/AtoN context):**
- `navigation.position` — JSON: `{"longitude": X, "latitude": Y}`
- `navigation.courseOverGroundTrue` — radians
- `navigation.speedOverGround` — m/s
- `navigation.headingTrue` — radians
- `name` — vessel/AtoN name
- `communication.callsignVhf` — callsign
- `design.aisShipType` — integer type code
- `navigation.destination.commonName` — destination

**Input (from SK server, updates transponder):**
- `navigation.destination.commonName` — destination
- `navigation.destination.eta` — ISO 8601 timestamp
- `communication.crewCount` — persons on board

## Dependencies

- SensESP 3.2.0 — IoT framework (WiFi, web UI, Signal K)
- SensESP/NMEA0183 — NMEA 0183 sentence parsing
- NMEA2000-library v4.17.2 — NMEA 2000 message handling
- NMEA2000_twai — ESP32 TWAI (CAN) driver
- Adafruit SSD1306 v2.5.1 — OLED display
- FastLED 3.9.4 — RGB LED (SK6805)
- elapsedMillis v1.0.6 — Timing utilities
- esp_websocket_client — WebSocket support (Espressif component)
