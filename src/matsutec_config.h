#ifndef AIS_INTERFACE_SRC_MATSUTEC_CONFIG_H_
#define AIS_INTERFACE_SRC_MATSUTEC_CONFIG_H_

#include <elapsedMillis.h>
#include <sensesp/transforms/zip.h>

#include <tuple>

#include "ReactESP.h"
#include "matsutec_ha102_parser.h"
#include "sensesp.h"
#include "sensesp/system/async_response_handler.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/system/saveable.h"
#include "sensesp/system/semaphore_value.h"
#include "sensesp/system/serializable.h"
#include "sensesp/transforms/lambda_transform.h"

namespace ais_interface {

inline String MatsutecQueryMMSISentence() {
  String sentence = "$PAMC,Q,MID";
  sensesp::nmea0183::AddChecksum(sentence);
  return sentence;
}

inline String MatsutecConfigureMMSISentence(String& mmsi) {
  String sentence = "$PAMC,C,MID," + mmsi + ",000000000";
  sensesp::nmea0183::AddChecksum(sentence);
  return sentence;
}

inline String MatsutecQueryShipDataSentence() {
  String sentence = "$ECAIQ,SSD";
  sensesp::nmea0183::AddChecksum(sentence);
  return sentence;
}

inline String MatsutecQueryVoyageStaticDataSentence() {
  String sentence = "$ECAIQ,VSD";
  sensesp::nmea0183::AddChecksum(sentence);
  return sentence;
}

inline String SetStaticShipDataSentence(const char* callsign, const char* ship_name,
                                 int antenna_dist_to_bow,
                                 int antenna_dist_to_stern,
                                 int antenna_dist_to_port,
                                 int antenna_dist_to_starboard) {
  char buf[100];
  snprintf(buf, 100, "$AISSD,%s,%s,%03d,%03d,%02d,%02d,0,", callsign, ship_name,
           antenna_dist_to_bow, antenna_dist_to_stern, antenna_dist_to_port,
           antenna_dist_to_starboard);
  String sentence = buf;
  sensesp::nmea0183::AddChecksum(sentence);
  return sentence;
}

inline String SetVoyageStaticDataSentence(int ship_type_, float max_draught_,
                                   int persons_on_board_, String destination_,
                                   time_t arrival_time_,
                                   int navigational_status_) {
  char time_buf[sizeof "070709"];
  char buf[100];
  int arrival_mday;
  int arrival_month;
  int regional_app_flags = 0;
  if (arrival_time_ == 0) {
    // Time not known
    strcpy(time_buf, "246000");
    arrival_mday = 0;
    arrival_month = 0;
  } else {
    struct tm arrival_time_tm = {0};
    gmtime_r(&arrival_time_, &arrival_time_tm);
    strftime(time_buf, sizeof time_buf, "%H%M%S", &arrival_time_tm);
    arrival_mday = arrival_time_tm.tm_mday;
    arrival_month = arrival_time_tm.tm_mon + 1;
  }
  snprintf(buf, 100, "$AIVSD,%d,%.1f,%d,%s,%s,%d,%d,%d,%d", ship_type_,
           max_draught_, persons_on_board_, destination_.c_str(), time_buf,
           arrival_mday, arrival_month, navigational_status_,
           regional_app_flags);

  String sentence = buf;
  sensesp::nmea0183::AddChecksum(sentence);
  return sentence;
}

class MMSIConfig : public sensesp::Saveable, public sensesp::Serializable {
 public:
  MMSIConfig(String mmsi, std::shared_ptr<MatsutecMMSIParser> parser,
             Stream* serial, String config_path = "")
      : sensesp::Saveable(config_path),
        sensesp::Serializable(),
        mmsi_{mmsi},
        serial_{serial},
        parser_{parser} {
    load();

    parser_->connect_to(&response_semaphore_);

    parser_->connect_to(&config_response_handler_);
    config_response_handler_.connect_to(
        new sensesp::LambdaConsumer<sensesp::AsyncResponseStatus>(
            [this](sensesp::AsyncResponseStatus status) {
              ESP_LOGV("config_response_status", "status: %d", status);
              if (status == sensesp::AsyncResponseStatus::kSuccess) {
                String received_mmsi = parser_->mmsi_.get();
                if (received_mmsi != mmsi_) {
                  ESP_LOGE("MMSIConfig", "MMSI was not set correctly");
                }
              }
            }));
  }

  virtual bool to_json(JsonObject& doc) override {
    doc["mmsi"] = mmsi_;
    return true;
  }

  virtual bool from_json(const JsonObject& config) override {
    String expected_keys[] = {"mmsi"};
    for (auto& key : expected_keys) {
      if (!config[key].is<JsonVariant>()) {
        return false;
      }
    }
    mmsi_ = config["mmsi"].as<String>();
    return true;
  }

  virtual bool refresh() override {
    String sentence = MatsutecQueryMMSISentence();
    response_semaphore_.clear();
    sensesp::event_loop()->onDelay(0, [this, sentence]() {
      ESP_LOGD("MMSIConfig", "Sending sentence: %s", sentence.c_str());
      serial_->println(sentence);
    });
    if (!response_semaphore_.take(1000)) {
      return false;
    }

    mmsi_ = parser_->mmsi_.get();

    return true;
  }

  virtual bool load() override {
    return refresh();
  }

  virtual bool save() override {
    String sentence = MatsutecConfigureMMSISentence(mmsi_);
    response_semaphore_.clear();
    sensesp::event_loop()->onDelay(0, [this, sentence]() {
      ESP_LOGD("MMSIConfig", "Sending sentence: %s", sentence.c_str());
      serial_->println(sentence);
    });
    if (!response_semaphore_.take(1000)) {
      return false;
    }
    return true;
  }

  const String& get_mmsi() const { return mmsi_; }

 protected:
  String mmsi_ = "";
  std::shared_ptr<MatsutecMMSIParser> parser_;
  Stream* serial_;
  sensesp::SemaphoreValue<bool> response_semaphore_;
  sensesp::AsyncResponseHandler query_response_handler_{3000};
  sensesp::AsyncResponseHandler config_response_handler_{3000};
};

inline const String ConfigSchema(const MMSIConfig& obj) {
  return R"({
      "type": "object",
      "properties": {
        "mmsi": { "title": "MMSI", "type": "string" }
      }
    })";
}

class ShipDataConfig : public sensesp::Saveable, public sensesp::Serializable {
 public:
  ShipDataConfig(String callsign, String ship_name, int antenna_dist_to_bow,
                 int antenna_dist_to_stern, int antenna_dist_to_port,
                 int antenna_dist_to_starboard,
                 std::shared_ptr<StaticShipDataParser> query_response_parser,
                 Stream* serial, String config_path = "")
      : sensesp::Saveable(config_path),
        sensesp::Serializable(),
        callsign_{callsign},
        ship_name_{ship_name},
        antenna_dist_to_bow_{antenna_dist_to_bow},
        antenna_dist_to_stern_{antenna_dist_to_stern},
        antenna_dist_to_port_{antenna_dist_to_port},
        antenna_dist_to_starboard_{antenna_dist_to_starboard},
        serial_{serial},
        query_response_parser_{query_response_parser} {
    load();

    query_response_parser_->connect_to(&response_semaphore_);
  }

  inline virtual bool to_json(JsonObject& doc) override {
    doc["callsign"] = callsign_;
    doc["ship_name"] = ship_name_;
    doc["antenna_dist_to_bow"] = antenna_dist_to_bow_;
    doc["antenna_dist_to_stern"] = antenna_dist_to_stern_;
    doc["antenna_dist_to_port"] = antenna_dist_to_port_;
    doc["antenna_dist_to_starboard"] = antenna_dist_to_starboard_;
    return true;
  }

  inline virtual bool from_json(const JsonObject& config) override {
    String expected_keys[] = {"callsign",
                              "ship_name",
                              "antenna_dist_to_bow",
                              "antenna_dist_to_stern",
                              "antenna_dist_to_port",
                              "antenna_dist_to_starboard"};
    for (auto& key : expected_keys) {
      if (!config[key].is<JsonVariant>()) {
        return false;
      }
    }
    callsign_ = config["callsign"].as<String>();
    ship_name_ = config["ship_name"].as<String>();
    antenna_dist_to_bow_ = config["antenna_dist_to_bow"].as<int>();
    antenna_dist_to_stern_ = config["antenna_dist_to_stern"].as<int>();
    antenna_dist_to_port_ = config["antenna_dist_to_port"].as<int>();
    antenna_dist_to_starboard_ = config["antenna_dist_to_starboard"].as<int>();
    return true;
  }

  inline virtual bool refresh() override {
    String sentence = MatsutecQueryShipDataSentence();
    ESP_LOGD("ShipDataConfig", "Sending sentence: %s", sentence.c_str());
    response_semaphore_.clear();
    sensesp::event_loop()->onDelay(
        0, [this, sentence]() { serial_->println(sentence); });

    if (!response_semaphore_.take(3000)) {
      return false;
    }

    callsign_ = query_response_parser_->callsign_.get();
    ship_name_ = query_response_parser_->ship_name_.get();
    antenna_dist_to_bow_ =
        query_response_parser_->antenna_distance_to_bow_.get();
    antenna_dist_to_stern_ =
        query_response_parser_->antenna_distance_to_stern_.get();
    antenna_dist_to_port_ =
        query_response_parser_->antenna_distance_to_port_.get();
    antenna_dist_to_starboard_ =
        query_response_parser_->antenna_distance_to_starboard_.get();

    return true;
  }

  inline virtual bool save() override {
    String command_sentence = SetStaticShipDataSentence(
        callsign_.c_str(), ship_name_.c_str(), antenna_dist_to_bow_,
        antenna_dist_to_stern_, antenna_dist_to_port_,
        antenna_dist_to_starboard_);
    sensesp::event_loop()->onDelay(0, [this, command_sentence]() {
      ESP_LOGD("ShipDataConfig", "Sending sentence: %s",
               command_sentence.c_str());
      serial_->println(command_sentence);
    });
    if (!response_semaphore_.take(3000)) {
      return false;
    }

    callsign_ = query_response_parser_->callsign_.get();
    ship_name_ = query_response_parser_->ship_name_.get();
    antenna_dist_to_bow_ =
        query_response_parser_->antenna_distance_to_bow_.get();
    antenna_dist_to_stern_ =
        query_response_parser_->antenna_distance_to_stern_.get();
    antenna_dist_to_port_ =
        query_response_parser_->antenna_distance_to_port_.get();
    antenna_dist_to_starboard_ =
        query_response_parser_->antenna_distance_to_starboard_.get();

    return true;
  }

 protected:
  String callsign_ = "";
  String ship_name_ = "";
  int antenna_dist_to_bow_ = 0;
  int antenna_dist_to_stern_ = 0;
  int antenna_dist_to_port_ = 0;
  int antenna_dist_to_starboard_ = 0;
  std::shared_ptr<StaticShipDataParser> query_response_parser_;
  Stream* serial_;
  sensesp::SemaphoreValue<bool> response_semaphore_;
};

inline const String ConfigSchema(const ShipDataConfig& obj) {
  const char schema[] = R"({
      "type": "object",
      "properties": {
        "callsign": { "title": "Callsign", "type": "string" },
        "ship_name": { "title": "Ship Name", "type": "string" },
        "antenna_dist_to_bow": { "title": "GNSS Antenna Distance to Bow", "type": "integer" },
        "antenna_dist_to_stern": { "title": "GNSS Antenna Distance to Stern", "type": "integer" },
        "antenna_dist_to_port": { "title": "GNSS Antenna Distance to Port", "type": "integer" },
        "antenna_dist_to_starboard": { "title": "GNSS Antenna Distance to Starboard", "type": "integer" }
      }
    })";
  return schema;
}

class VoyageStaticDataConfig : public sensesp::FileSystemSaveable,
                               virtual public sensesp::Serializable {
 public:
  VoyageStaticDataConfig(
      int ship_type, float max_draught, int persons_on_board,
      String destination, time_t arrival_time, int navigational_status,
      std::shared_ptr<VoyageStaticDataParser> query_response_parser,
      Stream* serial, String config_path = "")
      : sensesp::FileSystemSaveable(config_path),
        sensesp::Serializable(),
        ship_type_{ship_type},
        max_draught_{max_draught},
        persons_on_board_{persons_on_board},
        destination_{destination},
        arrival_time_{arrival_time},
        navigational_status_{navigational_status},
        serial_{serial},
        query_response_parser_{query_response_parser} {
    load();
    query_response_parser_->connect_to(&response_semaphore_);
  }

  inline virtual bool to_json(JsonObject& doc) override {
    doc["ship_type"] = ship_type_;
    doc["max_draught"] = max_draught_;
    doc["persons_on_board"] = persons_on_board_;
    doc["destination"] = destination_;

    char buf[sizeof "2011-10-08T07:07:09Z"];
    strftime(buf, sizeof buf, "%FT%TZ", gmtime(&arrival_time_));

    doc["arrival_time"] = buf;
    doc["navigational_status"] = navigational_status_;
    return true;
  }

  inline virtual bool refresh() override {
    response_semaphore_.clear();
    sensesp::event_loop()->onDelay(0, [this]() {
      String sentence = MatsutecQueryVoyageStaticDataSentence();
      ESP_LOGD("VoyageStaticDataConfig", "Sending sentence: %s",
               sentence.c_str());
      serial_->println(sentence);
    });
    if (!response_semaphore_.take(3000)) {
      return false;
    }

    ship_type_ = query_response_parser_->ship_type_.get();
    max_draught_ = query_response_parser_->draught_.get();
    persons_on_board_ = query_response_parser_->persons_on_board_.get();
    destination_ = query_response_parser_->destination_.get();
    arrival_time_ = query_response_parser_->arrival_time_.get();
    navigational_status_ = query_response_parser_->navigational_status_.get();

    return true;
  }

  inline virtual bool load() override { return refresh(); }

  virtual bool from_json(const JsonObject& config) override {
    String expected_keys[] = {"ship_type",        "max_draught",
                              "persons_on_board", "destination",
                              "arrival_time",     "navigational_status"};
    for (auto& key : expected_keys) {
      if (!config[key].is<JsonVariant>()) {
        return false;
      }
    }
    ship_type_ = config["ship_type"].as<int>();
    max_draught_ = config["max_draught"].as<float>();
    persons_on_board_ = config["persons_on_board"].as<int>();
    destination_ = config["destination"].as<String>();
    arrival_time_ = config["arrival_time"].as<time_t>();
    navigational_status_ = config["navigational_status"].as<int>();
    return true;
  }

  virtual bool save() override {
    response_semaphore_.clear();
    sensesp::event_loop()->onDelay(0, [this]() {
      String command_sentence = SetVoyageStaticDataSentence(
          ship_type_, max_draught_, persons_on_board_, destination_,
          arrival_time_, navigational_status_);
      ESP_LOGD("VoyageStaticDataConfig", "Sending sentence: %s",
               command_sentence.c_str());
      serial_->println(command_sentence);
    });
    if (!response_semaphore_.take(3000)) {
      return false;
    }

    ship_type_ = query_response_parser_->ship_type_.get();
    max_draught_ = query_response_parser_->draught_.get();
    persons_on_board_ = query_response_parser_->persons_on_board_.get();
    destination_ = query_response_parser_->destination_.get();
    arrival_time_ = query_response_parser_->arrival_time_.get();
    navigational_status_ = query_response_parser_->navigational_status_.get();

    return true;
  }

 protected:
  int ship_type_ = 0;
  float max_draught_ = 0;
  int persons_on_board_ = 0;
  String destination_ = "";
  time_t arrival_time_;
  float second_ = 0;
  int navigational_status_ = 0;
  std::shared_ptr<VoyageStaticDataParser> query_response_parser_;
  Stream* serial_;
  sensesp::SemaphoreValue<bool> response_semaphore_;
};

inline const String ConfigSchema(const VoyageStaticDataConfig& obj) {
  const char schema[] = R"({
      "type": "object",
      "properties": {
        "ship_type": { "title": "Ship Type", "type": "integer" },
        "max_draught": { "title": "Maximum Draught", "type": "number" },
        "persons_on_board": { "title": "Persons on Board", "type": "integer" },
        "destination": { "title": "Destination", "type": "string" },
        "arrival_time": { "title": "Arrival Time", "type": "string" },
        "navigational_status": { "title": "Navigational Status", "type": "integer" }
      }
    })";
  return schema;
}

}  // namespace ais_interface

#endif  // AIS_INTERFACE_SRC_MATSUTEC_CONFIG_H_
