#ifndef AIS_SIGNALK_H_
#define AIS_SIGNALK_H_

#include <ArduinoJson.h>

#include "ais_conversions.h"
#include "ais_message_types.h"

namespace ais {

// Build the Signal K vessel context string for a given MMSI.
// e.g. "vessels.urn:mrn:imo:mmsi:477553000"
inline void build_vessel_context(uint32_t mmsi, char* out, size_t out_size) {
  snprintf(out, out_size, "vessels.urn:mrn:imo:mmsi:%09u", mmsi);
}

// Build the AtoN context string.
// e.g. "atons.urn:mrn:imo:mmsi:993456789"
inline void build_aton_context(uint32_t mmsi, char* out, size_t out_size) {
  snprintf(out, out_size, "atons.urn:mrn:imo:mmsi:%09u", mmsi);
}

// Serialize an AIS Class A position report to a Signal K delta JSON document.
inline void to_signalk_delta(const ClassAPositionReport& r,
                              JsonDocument& doc) {
  char context[64];
  build_vessel_context(r.mmsi, context, sizeof(context));
  doc["context"] = context;

  JsonObject update = doc["updates"].add<JsonObject>();
  update["source"]["label"] = "ais";
  JsonArray values = update["values"].to<JsonArray>();

  if (r.latitude <= 90.0 && r.longitude <= 180.0) {
    JsonObject pos = values.add<JsonObject>();
    pos["path"] = "navigation.position";
    pos["value"]["longitude"] = r.longitude;
    pos["value"]["latitude"] = r.latitude;
  }

  if (r.cog < 360.0) {
    JsonObject cog = values.add<JsonObject>();
    cog["path"] = "navigation.courseOverGroundTrue";
    cog["value"] = cog_to_radians(r.cog);
  }

  if (!std::isnan(r.sog)) {
    JsonObject sog = values.add<JsonObject>();
    sog["path"] = "navigation.speedOverGround";
    sog["value"] = sog_to_ms(r.sog);
  }

  if (r.heading < 360) {
    JsonObject hdg = values.add<JsonObject>();
    hdg["path"] = "navigation.headingTrue";
    hdg["value"] = heading_to_radians(r.heading);
  }

  if (r.nav_status <= 15) {
    JsonObject nav = values.add<JsonObject>();
    nav["path"] = "navigation.state";
    // Signal K nav state values
    static const char* kNavStates[] = {
        "motoring", "anchored",    "not under command",
        "restricted maneuverability", "constrained by draft",
        "moored",   "aground",     "fishing",
        "sailing",  "hazardous material high speed",
        nullptr, nullptr, nullptr, nullptr,
        "ais-sart", "not defined"};
    const char* state =
        r.nav_status < 16 && kNavStates[r.nav_status]
            ? kNavStates[r.nav_status]
            : "not defined";
    nav["value"] = state;
  }
}

// Serialize an AIS Class B position report to a Signal K delta JSON document.
inline void to_signalk_delta(const ClassBPositionReport& r,
                              JsonDocument& doc) {
  char context[64];
  build_vessel_context(r.mmsi, context, sizeof(context));
  doc["context"] = context;

  JsonObject update = doc["updates"].add<JsonObject>();
  update["source"]["label"] = "ais";
  JsonArray values = update["values"].to<JsonArray>();

  if (r.latitude <= 90.0 && r.longitude <= 180.0) {
    JsonObject pos = values.add<JsonObject>();
    pos["path"] = "navigation.position";
    pos["value"]["longitude"] = r.longitude;
    pos["value"]["latitude"] = r.latitude;
  }

  if (r.cog < 360.0) {
    JsonObject cog = values.add<JsonObject>();
    cog["path"] = "navigation.courseOverGroundTrue";
    cog["value"] = cog_to_radians(r.cog);
  }

  if (!std::isnan(r.sog)) {
    JsonObject sog = values.add<JsonObject>();
    sog["path"] = "navigation.speedOverGround";
    sog["value"] = sog_to_ms(r.sog);
  }

  if (r.heading < 360) {
    JsonObject hdg = values.add<JsonObject>();
    hdg["path"] = "navigation.headingTrue";
    hdg["value"] = heading_to_radians(r.heading);
  }
}

// Serialize AIS Class A static/voyage data to a Signal K delta JSON document.
inline void to_signalk_delta(const ClassAStaticData& d, JsonDocument& doc) {
  char context[64];
  build_vessel_context(d.mmsi, context, sizeof(context));
  doc["context"] = context;

  JsonObject update = doc["updates"].add<JsonObject>();
  update["source"]["label"] = "ais";
  JsonArray values = update["values"].to<JsonArray>();

  if (d.name[0] != '\0') {
    JsonObject name = values.add<JsonObject>();
    name["path"] = "name";
    name["value"] = d.name;
  }

  if (d.callsign[0] != '\0') {
    JsonObject cs = values.add<JsonObject>();
    cs["path"] = "communication.callsignVhf";
    cs["value"] = d.callsign;
  }

  {
    JsonObject st = values.add<JsonObject>();
    st["path"] = "design.aisShipType";
    st["value"] = d.ship_type;
  }

  if (d.imo != 0) {
    JsonObject imo = values.add<JsonObject>();
    imo["path"] = "registrations.imo";
    char imo_str[32];
    snprintf(imo_str, sizeof(imo_str), "IMO %u", d.imo);
    imo["value"] = imo_str;
  }

  if (d.draught > 0) {
    JsonObject dr = values.add<JsonObject>();
    dr["path"] = "design.draft";
    JsonObject val = dr["value"].to<JsonObject>();
    val["value"] = d.draught;
    val["reference"] = "surface";
  }

  if (d.destination[0] != '\0') {
    JsonObject dest = values.add<JsonObject>();
    dest["path"] = "navigation.destination.commonName";
    dest["value"] = d.destination;
  }

  auto dims = dimensions_to_n2k(d.to_bow, d.to_stern, d.to_port,
                                 d.to_starboard);
  if (dims.length > 0) {
    JsonObject len = values.add<JsonObject>();
    len["path"] = "design.length";
    JsonObject val = len["value"].to<JsonObject>();
    val["overall"] = dims.length;
  }
  if (dims.beam > 0) {
    JsonObject beam = values.add<JsonObject>();
    beam["path"] = "design.beam";
    beam["value"] = dims.beam;
  }
}

// Serialize AIS Class B static data to a Signal K delta JSON document.
inline void to_signalk_delta(const ClassBStaticData& d, JsonDocument& doc) {
  char context[64];
  build_vessel_context(d.mmsi, context, sizeof(context));
  doc["context"] = context;

  JsonObject update = doc["updates"].add<JsonObject>();
  update["source"]["label"] = "ais";
  JsonArray values = update["values"].to<JsonArray>();

  if (d.part_number == 0 && d.name[0] != '\0') {
    JsonObject name = values.add<JsonObject>();
    name["path"] = "name";
    name["value"] = d.name;
  }

  if (d.part_number == 1) {
    if (d.callsign[0] != '\0') {
      JsonObject cs = values.add<JsonObject>();
      cs["path"] = "communication.callsignVhf";
      cs["value"] = d.callsign;
    }
    {
      JsonObject st = values.add<JsonObject>();
      st["path"] = "design.aisShipType";
      st["value"] = d.ship_type;
    }
  }
}

// Serialize AIS safety message to a Signal K delta JSON document.
inline void to_signalk_delta(const SafetyMessage& m, JsonDocument& doc) {
  char context[64];
  build_vessel_context(m.mmsi, context, sizeof(context));
  doc["context"] = context;

  JsonObject update = doc["updates"].add<JsonObject>();
  update["source"]["label"] = "ais";
  JsonArray values = update["values"].to<JsonArray>();

  JsonObject text = values.add<JsonObject>();
  text["path"] = "navigation.ais.safetyMessage";
  text["value"] = m.text;
}

// Serialize AIS AtoN report to a Signal K delta JSON document.
inline void to_signalk_delta(const AtoNReport& r, JsonDocument& doc) {
  char context[64];
  build_aton_context(r.mmsi, context, sizeof(context));
  doc["context"] = context;

  JsonObject update = doc["updates"].add<JsonObject>();
  update["source"]["label"] = "ais";
  JsonArray values = update["values"].to<JsonArray>();

  if (r.name[0] != '\0') {
    JsonObject name = values.add<JsonObject>();
    name["path"] = "name";
    name["value"] = r.name;
  }

  if (r.latitude <= 90.0 && r.longitude <= 180.0) {
    JsonObject pos = values.add<JsonObject>();
    pos["path"] = "navigation.position";
    pos["value"]["longitude"] = r.longitude;
    pos["value"]["latitude"] = r.latitude;
  }

  {
    JsonObject type = values.add<JsonObject>();
    type["path"] = "atonType";
    type["value"] = r.aton_type;
  }
}

}  // namespace ais

#endif  // AIS_SIGNALK_H_
