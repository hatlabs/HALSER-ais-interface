#ifndef AIS_VDM_PARSER_H_
#define AIS_VDM_PARSER_H_

#include <Arduino.h>

#include "ais_decoder.h"
#include "ais_reassembler.h"
#include "sensesp/system/observable.h"
#include "sensesp_nmea0183/sentence_parser/sentence_parser.h"

namespace ais {

// SentenceParser for AIS VDM (received) and VDO (own ship) sentences.
// Handles multi-part reassembly and decoding into typed AIS message structs.
//
// VDM sentence format:
//   !AIVDM,total,fragnum,seqid,channel,payload,fillbits*checksum
//   field:   0     1       2      3       4        5
class AISVDMSentenceParser : public sensesp::nmea0183::SentenceParser {
 public:
  AISVDMSentenceParser(sensesp::nmea0183::NMEA0183Parser* nmea,
                       const char* address = "..VDM")
      : SentenceParser(nmea), address_(address) {}

  const char* sentence_address() override { return address_; }

  bool parse_fields(const char* field_strings, const int field_offsets[],
                    int num_fields) override {
    if (num_fields < 6) return false;

    int total_fragments = atoi(field_strings + field_offsets[0]);
    int fragment_num = atoi(field_strings + field_offsets[1]);
    const char* seq_id_str = field_strings + field_offsets[2];
    int seq_id = seq_id_str[0] != '\0' ? atoi(seq_id_str) : 0;
    // field 3 = channel (not used for decoding)
    const char* payload = field_strings + field_offsets[4];
    int fill_bits = atoi(field_strings + field_offsets[5]);

    auto reassembled = reassembler_.add_fragment(
        total_fragments, fragment_num, seq_id, payload, fill_bits, millis());

    if (!reassembled.has_value()) {
      return fragment_num > 1;  // Intermediate fragments are OK
    }

    auto message =
        decode_payload(reassembled->payload, reassembled->fill_bits);
    if (!message.has_value()) return false;

    // Emit the decoded message to the appropriate observable
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
        message.value());

    return true;
  }

  // Observable outputs — one per AIS message type
  sensesp::ObservableValue<ClassAPositionReport> class_a_position_;
  sensesp::ObservableValue<ClassBPositionReport> class_b_position_;
  sensesp::ObservableValue<ClassAStaticData> class_a_static_;
  sensesp::ObservableValue<ClassBStaticData> class_b_static_;
  sensesp::ObservableValue<SafetyMessage> safety_message_;
  sensesp::ObservableValue<AtoNReport> aton_report_;

 private:
  const char* address_;
  AISReassembler reassembler_;
};

}  // namespace ais

#endif  // AIS_VDM_PARSER_H_
