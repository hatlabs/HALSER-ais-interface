#ifndef AIS_REASSEMBLER_H_
#define AIS_REASSEMBLER_H_

#include <cstdint>
#include <cstring>

namespace ais {

// Non-owning view of a completed reassembly result.
// Valid only until the next add_fragment() call.
struct ReassembledPayload {
  const char* payload;  // Pointer into the reassembler's internal buffer
  int fill_bits;
};

// Accumulates multi-part VDM/VDO sentence fragments into a complete payload.
// Framework-agnostic — uses a caller-provided timestamp for timeout.
class AISReassembler {
 public:
  // timeout_ms: discard incomplete messages after this many milliseconds.
  explicit AISReassembler(uint32_t timeout_ms = 2000)
      : timeout_ms_(timeout_ms) {
    reset();
  }

  // Process a fragment. Returns a pointer to the complete payload when all
  // fragments have been received, or nullptr if more fragments are needed.
  // The returned pointer is valid only until the next add_fragment() call.
  //
  // total_fragments: total number of fragments (1-5)
  // fragment_num: this fragment's number (1-based)
  // seq_id: sequential message ID (for correlating fragments)
  // payload: the 6-bit ASCII armored payload of this fragment
  // fill_bits: number of fill bits (only meaningful for last fragment)
  // now_ms: current time in milliseconds (for timeout detection)
  const ReassembledPayload* add_fragment(
      int total_fragments, int fragment_num, int seq_id,
      const char* payload, int fill_bits, uint32_t now_ms);

  void reset();

 private:
  uint32_t timeout_ms_;

  // Reassembly state
  int expected_fragments_;
  int received_fragments_;
  int seq_id_;
  int payload_len_;
  char payload_buffer_[512];
  int last_fill_bits_;
  uint32_t first_fragment_time_;

  // Result view — valid after a successful reassembly
  ReassembledPayload result_;
};

}  // namespace ais

#endif  // AIS_REASSEMBLER_H_
