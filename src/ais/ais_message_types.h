#ifndef AIS_MESSAGE_TYPES_H_
#define AIS_MESSAGE_TYPES_H_

#include <cstdint>

namespace ais {

struct ClassAPositionReport {
  uint8_t message_id;   // 1, 2, or 3
  uint8_t repeat;
  uint32_t mmsi;
  uint8_t nav_status;
  double rot;           // degrees/min (signed), NaN if not available
  double sog;           // knots
  bool accuracy;
  double longitude;     // degrees (signed), 181.0 if not available
  double latitude;      // degrees (signed), 91.0 if not available
  double cog;           // degrees, 360.0 if not available
  uint16_t heading;     // degrees, 511 if not available
  uint8_t timestamp;    // UTC second (0-59), 60+ = not available
  uint8_t maneuver;
  bool raim;
  uint32_t radio;
};

struct ClassBPositionReport {
  uint8_t message_id;   // 18 or 19
  uint8_t repeat;
  uint32_t mmsi;
  double sog;           // knots
  bool accuracy;
  double longitude;     // degrees (signed)
  double latitude;      // degrees (signed)
  double cog;           // degrees
  uint16_t heading;     // degrees, 511 if not available
  uint8_t timestamp;
  bool cs_unit;
  bool display;
  bool dsc;
  bool band;
  bool msg22;
  bool assigned;
  bool raim;
  uint32_t radio;
};

struct ClassAStaticData {
  uint8_t message_id;   // 5
  uint8_t repeat;
  uint32_t mmsi;
  uint8_t ais_version;
  uint32_t imo;
  char callsign[8];     // 7 chars + null
  char name[21];        // 20 chars + null
  uint8_t ship_type;
  uint16_t to_bow;
  uint16_t to_stern;
  uint16_t to_port;
  uint16_t to_starboard;
  uint8_t epfd;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  double draught;       // meters
  char destination[21]; // 20 chars + null
  bool dte;
};

struct ClassBStaticData {
  uint8_t message_id;   // 24
  uint8_t repeat;
  uint32_t mmsi;
  uint8_t part_number;  // 0=A, 1=B
  // Part A fields
  char name[21];
  // Part B fields
  uint8_t ship_type;
  char vendor_id[8];    // 7 chars + null
  char callsign[8];     // 7 chars + null
  uint16_t to_bow;
  uint16_t to_stern;
  uint16_t to_port;
  uint16_t to_starboard;
  uint32_t mothership_mmsi;
};

struct SafetyMessage {
  uint8_t message_id;   // 14
  uint8_t repeat;
  uint32_t mmsi;
  char text[162];       // up to 161 chars + null
};

struct AtoNReport {
  uint8_t message_id;   // 21
  uint8_t repeat;
  uint32_t mmsi;
  uint8_t aton_type;
  char name[21];
  bool accuracy;
  double longitude;
  double latitude;
  uint16_t to_bow;
  uint16_t to_stern;
  uint16_t to_port;
  uint16_t to_starboard;
  uint8_t epfd;
  uint8_t timestamp;
  bool off_position;
  uint8_t aton_status;
  bool raim;
  bool virtual_aton;
  bool assigned;
};

}  // namespace ais

#endif  // AIS_MESSAGE_TYPES_H_
