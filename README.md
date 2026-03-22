# HALSER AIS Interface

ESP32-C3 firmware for the [HALSER](https://shop.hatlabs.fi/products/halser) board that bridges a **Matsutec HA-102** AIS transponder to NMEA 2000 and Signal K networks.

This firmware serves as both a ready-to-use application and a reference example for building custom SensESP-based marine interface firmware.

## Features

- Decodes AIS messages (Class A/B position and static data, safety messages, Aids to Navigation)
- Forwards decoded AIS data to NMEA 2000 as standard PGNs
- Outputs AIS target data to Signal K with per-vessel context
- Configures Matsutec HA-102 transponder via web UI:
  - MMSI
  - Static ship data (name, callsign, antenna distances)
  - Voyage data (ship type, draught, destination, ETA, persons on board, navigational status)
- Receive-only mode (disables transponder transmission while preserving configuration)
- Bidirectional Signal K sync for destination, ETA, and crew count
- OLED status display (hostname, IP address, uptime)
- RGB LED activity indicator
- OTA firmware updates
- NMEA 2000 watchdog with configurable auto-reboot

## Hardware Required

- [HALSER](https://shop.hatlabs.fi/products/halser) board
- [Matsutec HA-102](http://www.matsutec.com.cn/) AIS transponder
- NMEA 2000 network connection
- Optional: SSD1306 128x64 OLED display (I2C)

## Wiring

| HALSER Pin | Function |
|------------|----------|
| GPIO 2 | UART1 TX → Matsutec RX |
| GPIO 3 | UART1 RX ← Matsutec TX |
| GPIO 4 | CAN TX → NMEA 2000 |
| GPIO 5 | CAN RX ← NMEA 2000 |
| GPIO 6 | I2C SDA (OLED display) |
| GPIO 7 | I2C SCL (OLED display) |
| GPIO 8 | RGB LED (SK6805) |
| GPIO 9 | Button |

The Matsutec HA-102 communicates via NMEA 0183 at 38,400 bit/s (8N1).

## Usage

### Initial Setup

1. Flash the firmware to the HALSER board
2. The device creates a WiFi access point on first boot
3. Connect to the AP and configure your WiFi network credentials
4. Access the web UI at `http://ais.local`

### Configuring MMSI

Navigate to the **MMSI** section in the web UI and enter your vessel's nine-digit MMSI. The firmware sends the configured MMSI to the transponder via the proprietary `$PAMC,C,MID` command.

### Configuring Static Ship Data

The **Static Ship Data** section lets you configure:

- **Callsign** — VHF radio callsign (up to 7 characters)
- **Ship Name** — vessel name (up to 20 characters)
- **GNSS Antenna Distance to Bow/Stern/Port/Starboard** — antenna position relative to the hull (meters)

These values are stored in the transponder and transmitted as part of the AIS static data.

### Configuring Voyage Data

The **Voyage Static Data** section lets you configure:

- **Ship Type** — AIS ship type code (integer, e.g., 36 = sailing vessel)
- **Maximum Draught** — vessel draught in meters
- **Persons on Board** — crew count
- **Destination** — destination port name
- **Arrival Time** — estimated time of arrival (UTC)
- **Navigational Status** — current status (e.g., 0 = under way using engine)

Voyage data is also stored in the transponder. Some fields can be updated automatically from a Signal K server (see below).

### Receive-Only Mode

The **Operating Mode** section provides a **Receive Only** toggle. When enabled:

- The firmware sends MMSI `000000000` to the transponder, which disables AIS transmission
- The transponder continues to receive and forward AIS messages
- Your configured MMSI is preserved and will be restored when you switch back to Transmit+Receive mode

This is useful when you want to monitor AIS traffic without transmitting, for example during testing or in a marina.

### Signal K Integration

The firmware integrates with Signal K in two directions:

**Output (AIS → Signal K):** Decoded AIS targets are emitted as Signal K deltas with per-vessel context (e.g., `vessels.urn:mrn:imo:mmsi:477553000`). Paths include `navigation.position`, `navigation.courseOverGroundTrue`, `navigation.speedOverGround`, `navigation.headingTrue`, `name`, `communication.callsignVhf`, `design.aisShipType`, and `navigation.destination.commonName`.

**Input (Signal K → transponder):** The firmware listens to the Signal K server for updates to:
- `navigation.destination.commonName` — updates the transponder's destination
- `navigation.destination.eta` — updates the transponder's ETA (ISO 8601 format)
- `communication.crewCount` — updates persons on board

This allows other Signal K sources (e.g., a chart plotter or navigation app) to automatically update the transponder's voyage data.

### NMEA 2000 Watchdog

An optional watchdog can be enabled in the web UI under **NMEA 2000 Watchdog**. When enabled, the device reboots if no NMEA 2000 messages are received for two minutes. This helps recover from CAN bus lockups. The setting requires a device restart to take effect.

### OLED Display

If connected, a 128x64 SSD1306 OLED display shows:
- Line 1: Device hostname
- Line 2: WiFi IP address
- Line 3: Uptime in seconds

## Supported AIS Message Types

| AIS Type | Description | NMEA 2000 PGN |
|----------|-------------|----------------|
| 1, 2, 3 | Class A Position Report | 129038 |
| 5 | Class A Static and Voyage Data | 129794 |
| 14 | Safety-Related Broadcast | 129802 |
| 18, 19 | Class B Position Report | 129039 |
| 21 | Aid-to-Navigation Report | 129041 |
| 24 Part A | Class B Static Data (name) | 129809 |
| 24 Part B | Class B Static Data (ship type, callsign, dimensions) | 129810 |

## Architecture

```
Matsutec HA-102 (NMEA 0183, 38,400 bit/s)
  │
  │ UART1 (GPIO 3 RX / GPIO 2 TX)
  ▼
NMEA0183IOTask
  ├── Matsutec config parsers (MMSI, ship data, voyage data)
  ├── RMC parser (GNSS time sync)
  └── AIS VDM/VDO parser
        └── AISReassembler (multi-part message assembly)
              └── AIS Decoder (6-bit binary → typed structs)
                    ├── N2K Senders → NMEA 2000 bus (TWAI, GPIO 4/5)
                    └── SK Output  → Signal K server (per-vessel context)

Web UI (SensESP) ──── Config objects ──── Matsutec (serial commands)
                                     └── Filesystem (voyage data backup)

Signal K server ──── SKValueListeners ──── Voyage data config ──── Matsutec
```

The firmware is built on [SensESP](https://github.com/SignalK/SensESP), which provides WiFi connectivity, a web UI for configuration, Signal K protocol support, and OTA updates. The AIS decoding layer (`src/ais/`) is framework-agnostic and has no SensESP or Arduino dependencies, making it reusable in other projects.

### Key Design Patterns

**Producer/Consumer pipeline:** SensESP uses a reactive pipeline where producers emit values that flow to connected consumers. The AIS VDM parser is a producer of typed message structs that are consumed by N2K senders and Signal K outputs. Producers and consumers are connected via `connect_to()`.

**Configuration as transponder state:** Unlike typical embedded configs stored in flash, the Matsutec configuration (MMSI, ship data) lives in the transponder itself. The config objects query the transponder on startup and send commands on save, using an async request/response pattern with timeout handling.

**Contextual Signal K output:** Standard SensESP outputs emit to a fixed Signal K path for the local vessel. AIS data needs dynamic context — each target vessel gets its own Signal K context based on MMSI. The `SKContextualOutput` class extends SensESP to support this.

## Code Structure

### AIS Decoding (`src/ais/`)

Framework-agnostic AIS message decoding with no Arduino/SensESP dependencies:

| File | Purpose |
|------|---------|
| `ais_message_types.h` | Data structs for 6 AIS message types |
| `ais_decoder.h/.cpp` | Binary payload decoding (ITU 6-bit armored → typed structs) |
| `ais_reassembler.h/.cpp` | Multi-part VDM sentence reassembly with timeout |
| `ais_vdm_fields.h` | VDM sentence field extraction and decode orchestration |
| `ais_vdm_parser.h` | SensESP `SentenceParser` integration with per-message-type producer outputs |
| `ais_conversions.h` | Unit conversions (AIS → NMEA 2000 / SI units) |

### Matsutec Configuration (`src/`)

| File | Purpose |
|------|---------|
| `matsutec_config.h` | Config objects for MMSI, ship data, voyage data (query/set via serial) |
| `matsutec_ha102_parser.h` | Parsers for proprietary Matsutec response sentences |
| `operating_mode.h` | Framework-agnostic TX/RX mode state machine |
| `operating_mode_config.h` | SensESP config wrapper for operating mode |

### Output (`src/sender/`, `src/signalk/`)

| File | Purpose |
|------|---------|
| `sender/ais_n2k_senders.h` | AIS → NMEA 2000 PGN senders (one class per message type) |
| `signalk/sk_ais_output.h` | AIS → Signal K contextual output mapper |
| `signalk/sk_contextual_output.h` | SensESP extension for dynamic Signal K context |

### Application

| File | Purpose |
|------|---------|
| `main.cpp` | Application entry point — wires all components together |
| `ssd1306_display.h/.cpp` | OLED display driver (hostname, IP, uptime) |

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
# Build firmware
pio run

# Upload to connected board
pio run -t upload

# Monitor serial output
pio device monitor
```

## Testing

Unit tests run on the native (x86_64) platform:

```bash
# Run all tests
pio test -e native
```

Test suites cover AIS decoding, unit conversions, multi-part reassembly, VDM field parsing, Signal K output, and operating mode logic.

## Using as a Template

This firmware demonstrates several patterns useful for building custom SensESP marine interfaces:

1. **NMEA 0183 sentence parsing** — Custom `SentenceParser` subclasses for proprietary and standard sentences
2. **NMEA 2000 output** — `ValueConsumer` classes that convert parsed data to N2K PGNs
3. **Signal K contextual output** — Emitting data to dynamic Signal K contexts (per-vessel, per-AtoN)
4. **External device configuration** — Query/response config pattern with async timeout handling
5. **Framework-agnostic core logic** — Keeping protocol decoding independent of the IoT framework for testability

To adapt this for a different device:
- Replace the Matsutec parsers and config classes with your device's protocol
- Modify the AIS decoder if supporting different message types
- Update the N2K sender classes for your target PGNs
- Adjust pin assignments in `main.cpp`

## License

See [LICENSE](LICENSE) for details.
