#ifndef SK_AIS_OUTPUT_H_
#define SK_AIS_OUTPUT_H_

#include <ArduinoJson.h>

#include "ais/ais_message_types.h"
#include "ais/ais_signalk.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/signalk/signalk_ws_client.h"

namespace ais {

// Sends decoded AIS messages as Signal K delta messages via WebSocket.
// Each AIS message is serialized to a JSON delta with the appropriate
// vessel/aton context and sent directly through the SK WebSocket client.
class SKAISOutput {
 public:
  SKAISOutput(std::shared_ptr<sensesp::SKWSClient> ws_client)
      : ws_client_(ws_client) {}

  template <typename T>
  void send(const T& msg) {
    JsonDocument doc;
    to_signalk_delta(msg, doc);

    String output;
    serializeJson(doc, output);
    ws_client_->sendTXT(output);
  }

  // Create ValueConsumer instances that forward to this output.
  // Call these after construction to get consumers for connect_to().

  std::shared_ptr<sensesp::LambdaConsumer<ClassAPositionReport>>
  class_a_position_consumer() {
    return std::make_shared<sensesp::LambdaConsumer<ClassAPositionReport>>(
        [this](ClassAPositionReport r) { send(r); });
  }

  std::shared_ptr<sensesp::LambdaConsumer<ClassBPositionReport>>
  class_b_position_consumer() {
    return std::make_shared<sensesp::LambdaConsumer<ClassBPositionReport>>(
        [this](ClassBPositionReport r) { send(r); });
  }

  std::shared_ptr<sensesp::LambdaConsumer<ClassAStaticData>>
  class_a_static_consumer() {
    return std::make_shared<sensesp::LambdaConsumer<ClassAStaticData>>(
        [this](ClassAStaticData d) { send(d); });
  }

  std::shared_ptr<sensesp::LambdaConsumer<ClassBStaticData>>
  class_b_static_consumer() {
    return std::make_shared<sensesp::LambdaConsumer<ClassBStaticData>>(
        [this](ClassBStaticData d) { send(d); });
  }

  std::shared_ptr<sensesp::LambdaConsumer<SafetyMessage>>
  safety_message_consumer() {
    return std::make_shared<sensesp::LambdaConsumer<SafetyMessage>>(
        [this](SafetyMessage m) { send(m); });
  }

  std::shared_ptr<sensesp::LambdaConsumer<AtoNReport>>
  aton_report_consumer() {
    return std::make_shared<sensesp::LambdaConsumer<AtoNReport>>(
        [this](AtoNReport r) { send(r); });
  }

 private:
  std::shared_ptr<sensesp::SKWSClient> ws_client_;
};

}  // namespace ais

#endif  // SK_AIS_OUTPUT_H_
