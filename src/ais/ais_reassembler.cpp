#include "ais_reassembler.h"

namespace ais {

void AISReassembler::reset() {
  expected_fragments_ = 0;
  received_fragments_ = 0;
  seq_id_ = -1;
  payload_len_ = 0;
  payload_buffer_[0] = '\0';
  last_fill_bits_ = 0;
  first_fragment_time_ = 0;
}

std::optional<ReassembledPayload> AISReassembler::add_fragment(
    int total_fragments, int fragment_num, int seq_id,
    const char* payload, int fill_bits, uint32_t now_ms) {
  // Single-part message — return immediately
  if (total_fragments == 1 && fragment_num == 1) {
    ReassembledPayload result{};
    strncpy(result.payload, payload, sizeof(result.payload) - 1);
    result.payload[sizeof(result.payload) - 1] = '\0';
    result.fill_bits = fill_bits;
    reset();
    return result;
  }

  // Check for timeout on in-progress reassembly
  if (received_fragments_ > 0 && (now_ms - first_fragment_time_) > timeout_ms_) {
    reset();
  }

  // First fragment of a new message
  if (fragment_num == 1) {
    reset();
    expected_fragments_ = total_fragments;
    seq_id_ = seq_id;
    first_fragment_time_ = now_ms;
  }

  // Validate: must be continuing an in-progress reassembly
  if (expected_fragments_ == 0) return std::nullopt;  // No reassembly in progress
  if (fragment_num != received_fragments_ + 1) {
    // Out of order — discard and reset
    reset();
    return std::nullopt;
  }
  if (seq_id != seq_id_) {
    // Wrong sequence ID — discard and reset
    reset();
    return std::nullopt;
  }

  // Append payload
  int payload_len = strlen(payload);
  if (payload_len_ + payload_len >= static_cast<int>(sizeof(payload_buffer_)) - 1) {
    // Buffer overflow — discard
    reset();
    return std::nullopt;
  }
  memcpy(payload_buffer_ + payload_len_, payload, payload_len);
  payload_len_ += payload_len;
  payload_buffer_[payload_len_] = '\0';
  last_fill_bits_ = fill_bits;
  received_fragments_++;

  // Check if complete
  if (received_fragments_ == expected_fragments_) {
    ReassembledPayload result{};
    memcpy(result.payload, payload_buffer_, payload_len_ + 1);
    result.fill_bits = last_fill_bits_;
    reset();
    return result;
  }

  return std::nullopt;
}

}  // namespace ais
