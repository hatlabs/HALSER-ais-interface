#ifndef AIS_VDM_PARSER_H_
#define AIS_VDM_PARSER_H_

#include <Arduino.h>

#include "ais_vdm_fields.h"
#include "sensesp/system/observable.h"
#include "sensesp_nmea0183/sentence_parser/sentence_parser.h"

namespace ais {

// SentenceParser for AIS VDM (received) and VDO (own ship) sentences.
// Delegates field parsing and decoding to parse_vdm_fields() and dispatches
// decoded messages to typed ObservableValue outputs.
class AISVDMSentenceParser : public sensesp::nmea0183::SentenceParser {
 public:
  AISVDMSentenceParser(sensesp::nmea0183::NMEA0183Parser* nmea,
                       const char* address = "..VDM")
      : SentenceParser(nmea), address_(address) {}

  const char* sentence_address() override { return address_; }

  bool parse_fields(const char* field_strings, const int field_offsets[],
                    int num_fields) override {
    auto result = parse_vdm_fields(field_strings, field_offsets, num_fields,
                                   reassembler_, millis());
    if (!result.has_value()) return false;

    return std::visit(
        [this](auto&& val) -> bool {
          using T = std::decay_t<decltype(val)>;
          if constexpr (std::is_same_v<T, VDMParseIncomplete>) {
            return true;  // Intermediate fragment accepted
          } else if constexpr (std::is_same_v<T, AISMessage>) {
            dispatch(val);
            return true;
          }
          return false;
        },
        result.value());
  }

  // Observable outputs — one per AIS message type
  sensesp::ObservableValue<ClassAPositionReport> class_a_position_;
  sensesp::ObservableValue<ClassBPositionReport> class_b_position_;
  sensesp::ObservableValue<ClassAStaticData> class_a_static_;
  sensesp::ObservableValue<ClassBStaticData> class_b_static_;
  sensesp::ObservableValue<SafetyMessage> safety_message_;
  sensesp::ObservableValue<AtoNReport> aton_report_;

 private:
  void dispatch(const AISMessage& message) {
    std::visit(
        [this](auto&& msg) {
          using T = std::decay_t<decltype(msg)>;
          if constexpr (std::is_same_v<T, ClassAPositionReport>) {
            class_a_position_.set(msg);
          } else if constexpr (std::is_same_v<T, ClassBPositionReport>) {
            class_b_position_.set(msg);
          } else if constexpr (std::is_same_v<T, ClassAStaticData>) {
            class_a_static_.set(msg);
          } else if constexpr (std::is_same_v<T, ClassBStaticData>) {
            class_b_static_.set(msg);
          } else if constexpr (std::is_same_v<T, SafetyMessage>) {
            safety_message_.set(msg);
          } else if constexpr (std::is_same_v<T, AtoNReport>) {
            aton_report_.set(msg);
          }
        },
        message);
  }

  const char* address_;
  AISReassembler reassembler_;
};

}  // namespace ais

#endif  // AIS_VDM_PARSER_H_
