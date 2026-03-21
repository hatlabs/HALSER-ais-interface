#ifndef AIS_VDM_FIELDS_H_
#define AIS_VDM_FIELDS_H_

#include <cstdlib>
#include <optional>

#include "ais_decoder.h"
#include "ais_reassembler.h"

namespace ais {

// Parse result for intermediate fragments (not yet complete).
struct VDMParseIncomplete {};

// Result of parse_vdm_fields: either a decoded message, an incomplete
// fragment indication, or failure (nullopt).
using VDMParseResult = std::optional<std::variant<AISMessage, VDMParseIncomplete>>;

// Parse VDM/VDO sentence fields into a decoded AIS message.
// Framework-agnostic — no SensESP or Arduino dependencies.
//
// field_strings: null-separated field values (from SentenceParser)
// field_offsets: byte offsets to each field in field_strings
// num_fields: number of fields (must be >= 6)
// reassembler: multi-part reassembly state
// now_ms: current time in milliseconds (for reassembly timeout)
//
// Returns:
//   - AISMessage variant on successful decode
//   - VDMParseIncomplete for valid intermediate fragments
//   - nullopt on error
inline VDMParseResult parse_vdm_fields(const char* field_strings,
                                        const int field_offsets[],
                                        int num_fields,
                                        AISReassembler& reassembler,
                                        uint32_t now_ms) {
  if (num_fields < 6) return std::nullopt;

  int total_fragments = atoi(field_strings + field_offsets[0]);
  int fragment_num = atoi(field_strings + field_offsets[1]);
  const char* seq_id_str = field_strings + field_offsets[2];
  int seq_id = seq_id_str[0] != '\0' ? atoi(seq_id_str) : 0;
  // field 3 = channel (not used for decoding)
  const char* payload = field_strings + field_offsets[4];
  int fill_bits = atoi(field_strings + field_offsets[5]);

  const auto* reassembled = reassembler.add_fragment(
      total_fragments, fragment_num, seq_id, payload, fill_bits, now_ms);

  if (reassembled == nullptr) {
    if (total_fragments > 1) {
      return VDMParseIncomplete{};  // Valid intermediate fragment
    }
    return std::nullopt;
  }

  auto message = decode_payload(reassembled->payload, reassembled->fill_bits);
  if (!message.has_value()) return std::nullopt;

  return message.value();
}

}  // namespace ais

#endif  // AIS_VDM_FIELDS_H_
