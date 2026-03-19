#ifndef AIS_INTERFACE_SRC_MATSUTEC_HA102_PARSER_H_
#define AIS_INTERFACE_SRC_MATSUTEC_HA102_PARSER_H_

#include "sensesp.h"
#include "sensesp/system/observablevalue.h"
#include "sensesp/system/valueconsumer.h"
#include "sensesp_nmea0183/nmea0183.h"
#include "sensesp_nmea0183/sentence_parser/field_parsers.h"
#include "sensesp_nmea0183/sentence_parser/sentence_parser.h"

namespace ais_interface {

class MatsutecMMSIParser : public sensesp::nmea0183::SentenceParser {
 public:
  MatsutecMMSIParser(sensesp::nmea0183::NMEA0183Parser &nmea0183)
      : sensesp::nmea0183::SentenceParser(&nmea0183) {}

  virtual const char *sentence_address() override final { return "PAMC,R,MID"; }

  bool parse_fields(const char *field_strings, const int field_offsets[],
                    int num_fields) override final {
    // Example sentence: $PAMC,R,MID,123456789,000000000*0D

    bool ok = true;

    if (num_fields != 5) {
      return false;
    }

    // 3 - MMSI, 9 digits

    String mmsi;
    ok &= sensesp::nmea0183::ParseString(
        &mmsi, field_strings + field_offsets[3], false);

    // 2 - Unknown, 9 zeros

    String zeros;
    ok &= sensesp::nmea0183::ParseString(
        &zeros, field_strings + field_offsets[4], false);

    if (zeros != "000000000") {
      return false;
    }

    mmsi_.set(mmsi);

    return true;
  }

  sensesp::ObservableValue<String> mmsi_;

 protected:
};

class StaticShipDataParser : public sensesp::nmea0183::SentenceParser {
 public:
  StaticShipDataParser(sensesp::nmea0183::NMEA0183Parser &nmea0183)
      : sensesp::nmea0183::SentenceParser(&nmea0183) {}

  virtual const char *sentence_address() override final { return "AISSD"; }

  bool parse_fields(const char *field_strings, const int field_offsets[],
                    int num_fields) override final {
    // Example sentence: $AISSD,ZYXW   ,ABCDEFGHIJKLMNOPQRS ,011,023,34,56,0,*35

    bool ok = true;

    String callsign;
    String ship_name;
    int antenna_distance_to_bow;
    int antenna_distance_to_stern;
    int antenna_distance_to_port;
    int antenna_distance_to_starboard;
    int dummy;

    ESP_LOGV("StaticShipDataParser::parse_fields", "num_fields: %d",
             num_fields);

    if (num_fields != 9) {
      return false;
    }

    ok &= sensesp::nmea0183::ParseString(
        &callsign, field_strings + field_offsets[1], false);

    ESP_LOGV("StaticShipDataParser::parse_fields", "callsign %d: %s", ok,
             callsign.c_str());

    ok &= sensesp::nmea0183::ParseString(
        &ship_name, field_strings + field_offsets[2], false);

    ESP_LOGV("StaticShipDataParser::parse_fields", "ship_name %d: %s", ok,
             ship_name.c_str());

    ok &= sensesp::nmea0183::ParseInt(&antenna_distance_to_bow,
                                      field_strings + field_offsets[3], false);

    ESP_LOGV("StaticShipDataParser::parse_fields",
             "antenna_distance_to_bow %d: %d", ok, antenna_distance_to_bow);

    ok &= sensesp::nmea0183::ParseInt(&antenna_distance_to_stern,
                                      field_strings + field_offsets[4], false);

    ESP_LOGV("StaticShipDataParser::parse_fields",
             "antenna_distance_to_stern %d: %d", ok, antenna_distance_to_stern);

    ok &= sensesp::nmea0183::ParseInt(&antenna_distance_to_port,
                                      field_strings + field_offsets[5], false);

    ESP_LOGV("StaticShipDataParser::parse_fields",
             "antenna_distance_to_port %d: %d", ok, antenna_distance_to_port);

    ok &= sensesp::nmea0183::ParseInt(&antenna_distance_to_starboard,
                                      field_strings + field_offsets[6], false);

    ESP_LOGV("StaticShipDataParser::parse_fields",
             "antenna_distance_to_starboard %d: %d", ok,
             antenna_distance_to_starboard);

    ok &= sensesp::nmea0183::ParseInt(&dummy, field_strings + field_offsets[7],
                                      false);

    ESP_LOGV("StaticShipDataParser::parse_fields", "dummy %d: %d", ok, dummy);

    if (dummy != 0) {
      ok = false;
    }

    if (!ok) {
      return false;
    }

    callsign_.set(callsign);
    ship_name_.set(ship_name);
    antenna_distance_to_bow_.set(antenna_distance_to_bow);
    antenna_distance_to_stern_.set(antenna_distance_to_stern);
    antenna_distance_to_port_.set(antenna_distance_to_port);
    antenna_distance_to_starboard_.set(antenna_distance_to_starboard);

    return true;
  }

  sensesp::ObservableValue<String> callsign_;
  sensesp::ObservableValue<String> ship_name_;
  sensesp::ObservableValue<int> antenna_distance_to_bow_;
  sensesp::ObservableValue<int> antenna_distance_to_stern_;
  sensesp::ObservableValue<int> antenna_distance_to_port_;
  sensesp::ObservableValue<int> antenna_distance_to_starboard_;

 protected:
};

class VoyageStaticDataParser : public sensesp::nmea0183::SentenceParser {
 public:
  VoyageStaticDataParser(sensesp::nmea0183::NMEA0183Parser &nmea0183)
      : sensesp::nmea0183::SentenceParser(&nmea0183) {}

  virtual const char *sentence_address() override final { return "AIVSD"; }

  bool parse_fields(const char *field_strings, const int field_offsets[],
                    int num_fields) override final {
    // Example sentence: $AIVSD,37,12,1000,SHENZHEN,134100,25,12,7

    bool ok = true;

    int ship_type;
    float draught;
    int persons_on_board;
    String destination = "";
    time_t arrival_time = 0;
    struct tm arrival_time_tm = {0};
    float second;
    int navigational_status;
    int regional_app_flags = 0;

    if (num_fields != 10) {
      return false;
    }

    ok &= sensesp::nmea0183::ParseInt(&ship_type,
                                      field_strings + field_offsets[1], false);
    ok &= sensesp::nmea0183::ParseFloat(
        &draught, field_strings + field_offsets[2], false);
    ok &= sensesp::nmea0183::ParseInt(&persons_on_board,
                                      field_strings + field_offsets[3], false);
    ok &= sensesp::nmea0183::ParseString(
        &destination, field_strings + field_offsets[4], true);
    ok &= sensesp::nmea0183::ParseTime(&arrival_time_tm.tm_hour,
                                       &arrival_time_tm.tm_min, &second,
                                       field_strings + field_offsets[5], true);
    ok &= sensesp::nmea0183::ParseInt(&arrival_time_tm.tm_mday,
                                      field_strings + field_offsets[6], false);
    ok &= sensesp::nmea0183::ParseInt(&arrival_time_tm.tm_mon,
                                      field_strings + field_offsets[7], false);
    ok &= sensesp::nmea0183::ParseInt(&navigational_status,
                                      field_strings + field_offsets[8], false);
    ok &= sensesp::nmea0183::ParseInt(&regional_app_flags,
                                      field_strings + field_offsets[9], true);

    // Ignore arrival time for now
    arrival_time = 0;

    if (!ok) {
      return false;
    }

    ship_type_.set(ship_type);
    draught_.set(draught);
    persons_on_board_.set(persons_on_board);
    destination_.set(destination);
    arrival_time_.set(arrival_time);
    navigational_status_.set(navigational_status);

    return true;
  }

  sensesp::ObservableValue<int> ship_type_;
  sensesp::ObservableValue<float> draught_;
  sensesp::ObservableValue<int> persons_on_board_;
  sensesp::ObservableValue<String> destination_;
  sensesp::ObservableValue<time_t> arrival_time_;
  sensesp::ObservableValue<int> navigational_status_;

 protected:
};

}  // namespace ais_interface

#endif  // AIS_INTERFACE_SRC_MATSUTEC_HA102_PARSER_H_
