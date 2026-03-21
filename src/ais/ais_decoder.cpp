#include "ais_decoder.h"

#include <cmath>
#include <cstring>

namespace ais {

int decode_armored_char(char c) {
  int v = static_cast<int>(c) - 48;
  if (v < 0) return -1;
  if (v > 39) v -= 8;
  if (v < 0 || v > 63) return -1;
  return v;
}

uint32_t extract_uint(const uint8_t* payload, int start_bit, int num_bits) {
  uint32_t result = 0;
  for (int i = 0; i < num_bits; i++) {
    int bit_index = start_bit + i;
    int byte_index = bit_index / 6;
    int bit_offset = 5 - (bit_index % 6);  // MSB first within each 6-bit unit
    uint8_t bit = (payload[byte_index] >> bit_offset) & 1;
    result = (result << 1) | bit;
  }
  return result;
}

int32_t extract_int(const uint8_t* payload, int start_bit, int num_bits) {
  uint32_t raw = extract_uint(payload, start_bit, num_bits);
  // Sign-extend: if the MSB is set, fill upper bits with 1s
  if (raw & (1u << (num_bits - 1))) {
    raw |= ~((1u << num_bits) - 1);
  }
  return static_cast<int32_t>(raw);
}

void extract_string(const uint8_t* payload, int start_bit, int num_chars,
                    char* out) {
  for (int i = 0; i < num_chars; i++) {
    uint32_t val = extract_uint(payload, start_bit + i * 6, 6);
    // AIS 6-bit ASCII: 0 = '@', 1-31 = 'A'-'_', 32 = ' ', 33-63 = mapped
    if (val < 32) {
      out[i] = static_cast<char>(val + 64);  // '@', 'A'..'Z', '['..'_'
    } else {
      out[i] = static_cast<char>(val);  // ' ', '!'..'?'
    }
  }
  out[num_chars] = '\0';

  // Strip trailing '@' padding and spaces
  for (int i = num_chars - 1; i >= 0; i--) {
    if (out[i] == '@' || out[i] == ' ') {
      out[i] = '\0';
    } else {
      break;
    }
  }
}

// Convert armored payload string to array of 6-bit values.
// Returns number of 6-bit values, or -1 on error.
static int unarmor_payload(const char* payload_str, uint8_t* out,
                           int max_out) {
  int len = 0;
  for (const char* p = payload_str; *p != '\0'; p++) {
    if (len >= max_out) return -1;
    int v = decode_armored_char(*p);
    if (v < 0) return -1;
    out[len++] = static_cast<uint8_t>(v);
  }
  return len;
}

static bool decode_class_a_position(const uint8_t* payload, int num_bits,
                                     ClassAPositionReport& out) {
  if (num_bits < 168) return false;

  out.message_id = extract_uint(payload, 0, 6);
  out.repeat = extract_uint(payload, 6, 2);
  out.mmsi = extract_uint(payload, 8, 30);
  out.nav_status = extract_uint(payload, 38, 4);

  int32_t rot_raw = extract_int(payload, 42, 8);
  if (rot_raw == -128) {
    out.rot = NAN;
  } else {
    // ROT = 4.733 * sqrt(ROT_sensor), stored as ROT_AIS
    // We store the raw value in deg/min for simplicity
    double rot_sign = rot_raw < 0 ? -1.0 : 1.0;
    double rot_abs = static_cast<double>(rot_raw < 0 ? -rot_raw : rot_raw);
    out.rot = rot_sign * (rot_abs / 4.733) * (rot_abs / 4.733);
  }

  uint32_t sog_raw = extract_uint(payload, 50, 10);
  out.sog = sog_raw == 1023 ? NAN : sog_raw / 10.0;

  out.accuracy = extract_uint(payload, 60, 1) != 0;

  int32_t lon_raw = extract_int(payload, 61, 28);
  out.longitude = lon_raw == 0x6791AC0 ? 181.0 : lon_raw / 600000.0;

  int32_t lat_raw = extract_int(payload, 89, 27);
  out.latitude = lat_raw == 0x3412140 ? 91.0 : lat_raw / 600000.0;

  uint32_t cog_raw = extract_uint(payload, 116, 12);
  out.cog = cog_raw == 3600 ? 360.0 : cog_raw / 10.0;

  out.heading = extract_uint(payload, 128, 9);
  out.timestamp = extract_uint(payload, 137, 6);
  out.maneuver = extract_uint(payload, 143, 2);
  out.raim = extract_uint(payload, 148, 1) != 0;
  out.radio = extract_uint(payload, 149, 19);

  return true;
}

static bool decode_class_a_static(const uint8_t* payload, int num_bits,
                                   ClassAStaticData& out) {
  if (num_bits < 424) return false;

  out.message_id = extract_uint(payload, 0, 6);
  out.repeat = extract_uint(payload, 6, 2);
  out.mmsi = extract_uint(payload, 8, 30);
  out.ais_version = extract_uint(payload, 38, 2);
  out.imo = extract_uint(payload, 40, 30);
  extract_string(payload, 70, 7, out.callsign);
  extract_string(payload, 112, 20, out.name);
  out.ship_type = extract_uint(payload, 232, 8);
  out.to_bow = extract_uint(payload, 240, 9);
  out.to_stern = extract_uint(payload, 249, 9);
  out.to_port = extract_uint(payload, 258, 6);
  out.to_starboard = extract_uint(payload, 264, 6);
  out.epfd = extract_uint(payload, 270, 4);
  out.month = extract_uint(payload, 274, 4);
  out.day = extract_uint(payload, 278, 5);
  out.hour = extract_uint(payload, 283, 5);
  out.minute = extract_uint(payload, 288, 6);

  uint32_t draught_raw = extract_uint(payload, 294, 8);
  out.draught = draught_raw / 10.0;

  extract_string(payload, 302, 20, out.destination);
  out.dte = extract_uint(payload, 422, 1) != 0;

  return true;
}

static bool decode_class_b_position(const uint8_t* payload, int num_bits,
                                     ClassBPositionReport& out) {
  if (num_bits < 168) return false;

  out.message_id = extract_uint(payload, 0, 6);
  out.repeat = extract_uint(payload, 6, 2);
  out.mmsi = extract_uint(payload, 8, 30);

  uint32_t sog_raw = extract_uint(payload, 46, 10);
  out.sog = sog_raw == 1023 ? NAN : sog_raw / 10.0;

  out.accuracy = extract_uint(payload, 56, 1) != 0;

  int32_t lon_raw = extract_int(payload, 57, 28);
  out.longitude = lon_raw == 0x6791AC0 ? 181.0 : lon_raw / 600000.0;

  int32_t lat_raw = extract_int(payload, 85, 27);
  out.latitude = lat_raw == 0x3412140 ? 91.0 : lat_raw / 600000.0;

  uint32_t cog_raw = extract_uint(payload, 112, 12);
  out.cog = cog_raw == 3600 ? 360.0 : cog_raw / 10.0;

  out.heading = extract_uint(payload, 124, 9);
  out.timestamp = extract_uint(payload, 133, 6);
  out.cs_unit = extract_uint(payload, 141, 1) != 0;
  out.display = extract_uint(payload, 142, 1) != 0;
  out.dsc = extract_uint(payload, 143, 1) != 0;
  out.band = extract_uint(payload, 144, 1) != 0;
  out.msg22 = extract_uint(payload, 145, 1) != 0;
  out.assigned = extract_uint(payload, 146, 1) != 0;
  out.raim = extract_uint(payload, 147, 1) != 0;
  out.radio = extract_uint(payload, 148, 20);

  return true;
}

static bool decode_safety_message(const uint8_t* payload, int num_bits,
                                   SafetyMessage& out) {
  if (num_bits < 40) return false;

  out.message_id = extract_uint(payload, 0, 6);
  out.repeat = extract_uint(payload, 6, 2);
  out.mmsi = extract_uint(payload, 8, 30);

  // Text starts at bit 40, each char is 6 bits
  int text_bits = num_bits - 40;
  int text_chars = text_bits / 6;
  if (text_chars > 161) text_chars = 161;
  if (text_chars > 0) {
    extract_string(payload, 40, text_chars, out.text);
  } else {
    out.text[0] = '\0';
  }

  return true;
}

static bool decode_class_b_static(const uint8_t* payload, int num_bits,
                                   ClassBStaticData& out) {
  if (num_bits < 160) return false;

  out.message_id = extract_uint(payload, 0, 6);
  out.repeat = extract_uint(payload, 6, 2);
  out.mmsi = extract_uint(payload, 8, 30);
  out.part_number = extract_uint(payload, 38, 2);

  if (out.part_number == 0) {
    // Part A: name
    extract_string(payload, 40, 20, out.name);
    // Zero out Part B fields
    out.ship_type = 0;
    out.vendor_id[0] = '\0';
    out.callsign[0] = '\0';
    out.to_bow = 0;
    out.to_stern = 0;
    out.to_port = 0;
    out.to_starboard = 0;
    out.mothership_mmsi = 0;
  } else if (out.part_number == 1) {
    // Part B
    out.name[0] = '\0';
    out.ship_type = extract_uint(payload, 40, 8);
    extract_string(payload, 48, 7, out.vendor_id);
    extract_string(payload, 90, 7, out.callsign);
    out.to_bow = extract_uint(payload, 132, 9);
    out.to_stern = extract_uint(payload, 141, 9);
    out.to_port = extract_uint(payload, 150, 6);
    out.to_starboard = extract_uint(payload, 156, 6);
    out.mothership_mmsi = extract_uint(payload, 132, 30);
  }

  return true;
}

static bool decode_aton_report(const uint8_t* payload, int num_bits,
                                AtoNReport& out) {
  if (num_bits < 272) return false;

  out.message_id = extract_uint(payload, 0, 6);
  out.repeat = extract_uint(payload, 6, 2);
  out.mmsi = extract_uint(payload, 8, 30);
  out.aton_type = extract_uint(payload, 38, 5);
  extract_string(payload, 43, 20, out.name);
  out.accuracy = extract_uint(payload, 163, 1) != 0;

  int32_t lon_raw = extract_int(payload, 164, 28);
  out.longitude = lon_raw == 0x6791AC0 ? 181.0 : lon_raw / 600000.0;

  int32_t lat_raw = extract_int(payload, 192, 27);
  out.latitude = lat_raw == 0x3412140 ? 91.0 : lat_raw / 600000.0;

  out.to_bow = extract_uint(payload, 219, 9);
  out.to_stern = extract_uint(payload, 228, 9);
  out.to_port = extract_uint(payload, 237, 6);
  out.to_starboard = extract_uint(payload, 243, 6);
  out.epfd = extract_uint(payload, 249, 4);
  out.timestamp = extract_uint(payload, 253, 6);
  out.off_position = extract_uint(payload, 259, 1) != 0;
  out.aton_status = extract_uint(payload, 260, 8);
  out.raim = extract_uint(payload, 268, 1) != 0;
  out.virtual_aton = extract_uint(payload, 269, 1) != 0;
  out.assigned = extract_uint(payload, 270, 1) != 0;

  return true;
}

std::optional<AISMessage> decode_payload(const char* payload_str,
                                          int fill_bits) {
  constexpr int kMaxPayloadBytes = 128;
  static uint8_t payload[kMaxPayloadBytes];  // static: called from single task

  int len = unarmor_payload(payload_str, payload, kMaxPayloadBytes);
  if (len < 1) return std::nullopt;

  int num_bits = len * 6 - fill_bits;
  if (num_bits < 6) return std::nullopt;

  uint8_t msg_type = extract_uint(payload, 0, 6);

  switch (msg_type) {
    case 1:
    case 2:
    case 3: {
      ClassAPositionReport report{};
      if (decode_class_a_position(payload, num_bits, report)) {
        return report;
      }
      return std::nullopt;
    }
    case 5: {
      ClassAStaticData data{};
      if (decode_class_a_static(payload, num_bits, data)) {
        return data;
      }
      return std::nullopt;
    }
    case 14: {
      SafetyMessage msg{};
      if (decode_safety_message(payload, num_bits, msg)) {
        return msg;
      }
      return std::nullopt;
    }
    case 18:
    case 19: {
      ClassBPositionReport report{};
      if (decode_class_b_position(payload, num_bits, report)) {
        return report;
      }
      return std::nullopt;
    }
    case 21: {
      AtoNReport report{};
      if (decode_aton_report(payload, num_bits, report)) {
        return report;
      }
      return std::nullopt;
    }
    case 24: {
      ClassBStaticData data{};
      if (decode_class_b_static(payload, num_bits, data)) {
        return data;
      }
      return std::nullopt;
    }
    default:
      return std::nullopt;
  }
}

}  // namespace ais
