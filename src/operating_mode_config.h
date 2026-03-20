#ifndef OPERATING_MODE_CONFIG_H_
#define OPERATING_MODE_CONFIG_H_

#include "matsutec_config.h"
#include "operating_mode.h"
#include "sensesp/system/saveable.h"
#include "sensesp/system/serializable.h"
#include "sensesp/ui/config_item.h"

namespace ais_interface {

// SensESP configuration for the operating mode (TX+RX vs RX-only).
// Coordinates with MMSIConfig to send the correct MMSI to the transponder.
class OperatingModeConfig : public sensesp::Saveable,
                            public sensesp::Serializable {
 public:
  OperatingModeConfig(std::shared_ptr<MMSIConfig> mmsi_config,
                      Stream* serial, String config_path = "")
      : sensesp::Saveable(config_path),
        mmsi_config_(mmsi_config),
        serial_(serial) {
    // Initialize logic with current MMSI from config
    logic_.set_user_mmsi(mmsi_config_->get_mmsi().c_str());
    load();
  }

  virtual bool to_json(JsonObject& doc) override {
    doc["receive_only"] =
        (logic_.get_mode() == OperatingMode::kReceiveOnly);
    return true;
  }

  virtual bool from_json(const JsonObject& config) override {
    if (!config["receive_only"].is<bool>()) {
      return false;
    }
    bool receive_only = config["receive_only"].as<bool>();
    auto new_mode = receive_only ? OperatingMode::kReceiveOnly
                                 : OperatingMode::kTransmitReceive;
    apply_mode(new_mode);
    return true;
  }

  OperatingMode get_mode() const { return logic_.get_mode(); }

  // Called when MMSIConfig changes the user's MMSI.
  void on_mmsi_changed(const String& new_mmsi) {
    auto mmsi_to_send = logic_.on_user_mmsi_changed(new_mmsi.c_str());
    if (mmsi_to_send.has_value()) {
      send_mmsi_to_transponder(mmsi_to_send.value());
    }
  }

 private:
  void apply_mode(OperatingMode new_mode) {
    auto mmsi_to_send = logic_.set_mode(new_mode);
    if (mmsi_to_send.has_value()) {
      send_mmsi_to_transponder(mmsi_to_send.value());
    }
  }

  void send_mmsi_to_transponder(const char* mmsi) {
    String mmsi_str(mmsi);
    String sentence = MatsutecConfigureMMSISentence(mmsi_str);
    sensesp::event_loop()->onDelay(0, [this, sentence]() {
      ESP_LOGD("OperatingMode", "Sending MMSI: %s", sentence.c_str());
      serial_->println(sentence);
    });
  }

  OperatingModeLogic logic_;
  std::shared_ptr<MMSIConfig> mmsi_config_;
  Stream* serial_;
};

inline const String ConfigSchema(const OperatingModeConfig& obj) {
  return R"({
    "type": "object",
    "properties": {
      "receive_only": {
        "title": "Receive Only",
        "type": "boolean",
        "description": "When enabled, the transponder will not transmit AIS data. The configured MMSI is preserved."
      }
    }
  })";
}

}  // namespace ais_interface

#endif  // OPERATING_MODE_CONFIG_H_
