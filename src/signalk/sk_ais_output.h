#ifndef SK_AIS_OUTPUT_H_
#define SK_AIS_OUTPUT_H_

#include <utility>

#include "ais/ais_conversions.h"
#include "ais/ais_message_types.h"
#include "sensesp/system/lambda_consumer.h"
#include "sk_contextual_output.h"

namespace ais {

// Wires AIS VDM parser outputs to Signal K contextual outputs.
// Creates one SKContextualOutput per SK path and provides
// LambdaConsumer instances that transform AIS structs into
// (context, value) pairs.
class SKAISOutput {
 public:
  SKAISOutput() {
    position_out_ =
        std::make_shared<sensesp::SKContextualOutputRawJson>(
            "navigation.position");
    cog_out_ = std::make_shared<sensesp::SKContextualOutput<double>>(
        "navigation.courseOverGroundTrue");
    sog_out_ = std::make_shared<sensesp::SKContextualOutput<double>>(
        "navigation.speedOverGround");
    heading_out_ = std::make_shared<sensesp::SKContextualOutput<double>>(
        "navigation.headingTrue");
    name_out_ = std::make_shared<sensesp::SKContextualOutput<String>>("name");
    callsign_out_ = std::make_shared<sensesp::SKContextualOutput<String>>(
        "communication.callsignVhf");
    ship_type_out_ = std::make_shared<sensesp::SKContextualOutput<int>>(
        "design.aisShipType");
    destination_out_ = std::make_shared<sensesp::SKContextualOutput<String>>(
        "navigation.destination.commonName");
  }

  // Connect to AIS VDM parser outputs
  std::shared_ptr<sensesp::LambdaConsumer<ClassAPositionReport>>
  class_a_position_consumer() {
    return std::make_shared<sensesp::LambdaConsumer<ClassAPositionReport>>(
        [this](ClassAPositionReport r) { emit_position(r); });
  }

  std::shared_ptr<sensesp::LambdaConsumer<ClassBPositionReport>>
  class_b_position_consumer() {
    return std::make_shared<sensesp::LambdaConsumer<ClassBPositionReport>>(
        [this](ClassBPositionReport r) { emit_position(r); });
  }

  std::shared_ptr<sensesp::LambdaConsumer<ClassAStaticData>>
  class_a_static_consumer() {
    return std::make_shared<sensesp::LambdaConsumer<ClassAStaticData>>(
        [this](ClassAStaticData d) {
          String ctx = vessel_context(d.mmsi);

          if (d.name[0] != '\0') {
            name_out_->set({ctx, String(d.name)});
          }
          if (d.callsign[0] != '\0') {
            callsign_out_->set({ctx, String(d.callsign)});
          }
          ship_type_out_->set({ctx, d.ship_type});
          if (d.destination[0] != '\0') {
            destination_out_->set({ctx, String(d.destination)});
          }
        });
  }

  std::shared_ptr<sensesp::LambdaConsumer<ClassBStaticData>>
  class_b_static_consumer() {
    return std::make_shared<sensesp::LambdaConsumer<ClassBStaticData>>(
        [this](ClassBStaticData d) {
          String ctx = vessel_context(d.mmsi);
          if (d.part_number == 0 && d.name[0] != '\0') {
            name_out_->set({ctx, String(d.name)});
          }
          if (d.part_number == 1) {
            if (d.callsign[0] != '\0') {
              callsign_out_->set({ctx, String(d.callsign)});
            }
            ship_type_out_->set({ctx, d.ship_type});
          }
        });
  }

  std::shared_ptr<sensesp::LambdaConsumer<SafetyMessage>>
  safety_message_consumer() {
    // Safety messages don't map cleanly to standard SK paths;
    // skip for now
    return std::make_shared<sensesp::LambdaConsumer<SafetyMessage>>(
        [](SafetyMessage) {});
  }

  std::shared_ptr<sensesp::LambdaConsumer<AtoNReport>>
  aton_report_consumer() {
    return std::make_shared<sensesp::LambdaConsumer<AtoNReport>>(
        [this](AtoNReport r) {
          String ctx = aton_context(r.mmsi);
          if (r.latitude <= 90.0 && r.longitude <= 180.0) {
            char json[64];
            snprintf(json, sizeof(json),
                     "{\"longitude\":%.6f,\"latitude\":%.6f}", r.longitude,
                     r.latitude);
            position_out_->set({ctx, String(json)});
          }
          if (r.name[0] != '\0') {
            name_out_->set({ctx, String(r.name)});
          }
        });
  }

 private:
  static String vessel_context(uint32_t mmsi) {
    char ctx[64];
    snprintf(ctx, sizeof(ctx), "vessels.urn:mrn:imo:mmsi:%09u", mmsi);
    return String(ctx);
  }

  static String aton_context(uint32_t mmsi) {
    char ctx[64];
    snprintf(ctx, sizeof(ctx), "atons.urn:mrn:imo:mmsi:%09u", mmsi);
    return String(ctx);
  }

  // Shared position emit logic for Class A and B reports
  template <typename T>
  void emit_position(const T& r) {
    String ctx = vessel_context(r.mmsi);

    if (r.latitude <= 90.0 && r.longitude <= 180.0) {
      char json[64];
      snprintf(json, sizeof(json), "{\"longitude\":%.6f,\"latitude\":%.6f}",
               r.longitude, r.latitude);
      position_out_->set({ctx, String(json)});
    }
    if (r.cog < 360.0) {
      cog_out_->set({ctx, cog_to_radians(r.cog)});
    }
    if (!std::isnan(r.sog)) {
      sog_out_->set({ctx, sog_to_ms(r.sog)});
    }
    if (r.heading < 360) {
      heading_out_->set({ctx, heading_to_radians(r.heading)});
    }
  }

  // SK contextual outputs — one per SK path
  std::shared_ptr<sensesp::SKContextualOutputRawJson> position_out_;
  std::shared_ptr<sensesp::SKContextualOutput<double>> cog_out_;
  std::shared_ptr<sensesp::SKContextualOutput<double>> sog_out_;
  std::shared_ptr<sensesp::SKContextualOutput<double>> heading_out_;
  std::shared_ptr<sensesp::SKContextualOutput<String>> name_out_;
  std::shared_ptr<sensesp::SKContextualOutput<String>> callsign_out_;
  std::shared_ptr<sensesp::SKContextualOutput<int>> ship_type_out_;
  std::shared_ptr<sensesp::SKContextualOutput<String>> destination_out_;
};

}  // namespace ais

#endif  // SK_AIS_OUTPUT_H_
