#ifndef AIS_INTERFACE_SRC_UI_CONFIG_H_
#define AIS_INTERFACE_SRC_UI_CONFIG_H_

#include "sensesp.h"
#include "sensesp/system/saveable.h"
#include "sensesp/system/serializable.h"

namespace ais_interface {

/**
 * @brief Config control with Enable checkbox and a Port input field.
 *
 */
class PortConfig : public sensesp::FileSystemSaveable,
                   virtual public sensesp::Serializable {
 public:
  PortConfig(bool enabled, uint16_t port, const String& config_path,
             const String& description, int sort_order = 1000)
      : enabled_(enabled),
        port_(port),
        sensesp::FileSystemSaveable(config_path),
        sensesp::Serializable() {
    load();
  }

  inline virtual bool to_json(JsonObject& doc) override {
    doc["enable"] = enabled_;
    doc["port"] = port_;
    return true;
  }

  inline virtual bool from_json(const JsonObject& doc) override {
    if (!doc["enable"].is<JsonVariant>() || !doc["port"].is<JsonVariant>()) {
      return false;
    }

    enabled_ = doc["enable"];
    port_ = doc["port"];

    return true;
  }

  bool get_enabled() { return enabled_; }
  uint16_t get_port() { return port_; }

 protected:
  bool enabled_ = false;
  int port_ = 0;
};

inline const String ConfigSchema(const PortConfig& obj) {
  return R"({
    "type": "object",
    "properties": {
        "enable": { "title": "Enable", "type": "boolean" },
        "port": { "title": "Port", "type": "integer" }
    }
  })";
}

class HostPortConfig : public sensesp::FileSystemSaveable,
                       virtual public sensesp::Serializable {
 public:
  HostPortConfig(bool enabled, const String& host, uint16_t port,
                 const String& enabled_title, const String& host_title,
                 const String& port_title, const String& config_path,
                 const String& description, int sort_order = 1000)
      : enabled_(enabled),
        host_(host),
        port_(port),
        enabled_title_(enabled_title),
        host_title_(host_title),
        port_title_(port_title),
        sensesp::FileSystemSaveable(config_path),
        sensesp::Serializable() {
    load();
  }

  inline virtual bool to_json(JsonObject& doc) override {
    doc["enable"] = enabled_;
    doc["host"] = host_;
    doc["port"] = port_;
    return true;
  }

  inline virtual bool from_json(const JsonObject& doc) override {
    if (!doc["enable"].is<JsonVariant>() || !doc["host"].is<JsonVariant>() ||
        !doc["port"].is<JsonVariant>()) {
      return false;
    }

    enabled_ = doc["enable"];
    host_ = doc["host"].as<String>();
    port_ = doc["port"];

    return true;
  }

  bool get_enabled() { return enabled_; }
  String get_host() { return host_; }
  uint16_t get_port() { return port_; }

 protected:
  bool enabled_ = false;
  String host_ = "";
  int port_ = 0;
  String enabled_title_;
  String host_title_;
  String port_title_;

  friend const String ConfigSchema(const HostPortConfig& obj);
};

inline const String ConfigSchema(const HostPortConfig& obj) {
  String schema = R"({
    "type": "object",
    "properties": {
        "enable": { "title": "{{title}}", "type": "boolean" },
        "host": { "title": "{{host}}", "type": "string" },
        "port": { "title": "{{port}}", "type": "integer" }
    }
  })";

  schema.replace("{{title}}", obj.enabled_title_);
  schema.replace("{{host}}", obj.host_title_);
  schema.replace("{{port}}", obj.port_title_);
  return schema;
}

}  // namespace ais_interface

#endif  // AIS_INTERFACE_SRC_UI_CONFIG_H_
