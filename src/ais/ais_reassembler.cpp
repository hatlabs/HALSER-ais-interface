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

const ReassembledPayload* AISReassembler::add_fragment(
    int total_fragments, int fragment_num, int seq_id,
    const char* payload, int fill_bits, uint32_t now_ms) {
  // Single-part message — copy to internal buffer, return view
  if (total_fragments == 1 && fragment_num == 1) {
    reset();
    int len = strlen(payload);
    if (len >= static_cast<int>(sizeof(payload_buffer_))) {
      return nullptr;
    }
    memcpy(payload_buffer_, payload, len + 1);
    payload_len_ = len;
    result_ = {payload_buffer_, fill_bits};
    return &result_;
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
  if (expected_fragments_ == 0) return nullptr;
  if (fragment_num != received_fragments_ + 1) {
    reset();
    return nullptr;
  }
  if (seq_id != seq_id_) {
    reset();
    return nullptr;
  }

  // Append payload
  int payload_len = strlen(payload);
  if (payload_len_ + payload_len >= static_cast<int>(sizeof(payload_buffer_)) - 1) {
    reset();
    return nullptr;
  }
  memcpy(payload_buffer_ + payload_len_, payload, payload_len);
  payload_len_ += payload_len;
  payload_buffer_[payload_len_] = '\0';
  last_fill_bits_ = fill_bits;
  received_fragments_++;

  // Check if complete
  if (received_fragments_ == expected_fragments_) {
    result_ = {payload_buffer_, last_fill_bits_};
    return &result_;
  }

  return nullptr;
}

}  // namespace ais
