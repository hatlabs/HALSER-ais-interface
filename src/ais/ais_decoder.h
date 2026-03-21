#ifndef AIS_DECODER_H_
#define AIS_DECODER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

#include "ais_message_types.h"

namespace ais {

// Result type for decode operations
using AISMessage = std::variant<
    ClassAPositionReport,
    ClassBPositionReport,
    ClassAStaticData,
    ClassBStaticData,
    SafetyMessage,
    AtoNReport>;

// Decode a 6-bit ASCII character to its 6-bit value (0-63).
// Returns -1 on invalid input.
int decode_armored_char(char c);

// Extract an unsigned integer from a 6-bit bitstream.
// payload: array of 6-bit values (from decode_armored_char)
// start_bit: bit offset (0 = MSB of first payload byte)
// num_bits: number of bits to extract (max 32)
uint32_t extract_uint(const uint8_t* payload, int start_bit, int num_bits);

// Extract a signed integer from a 6-bit bitstream (two's complement).
int32_t extract_int(const uint8_t* payload, int start_bit, int num_bits);

// Extract a 6-bit ASCII string from a 6-bit bitstream.
// out must have room for (num_chars + 1) bytes. Result is null-terminated
// and trailing '@' padding is stripped.
void extract_string(const uint8_t* payload, int start_bit, int num_chars,
                    char* out);

// Decode an AIS payload into a typed message struct.
// payload_str: the 6-bit ASCII armored payload string from the VDM sentence
// fill_bits: number of fill bits (0-5) from the VDM sentence
// Returns the decoded message, or std::nullopt on failure.
// The caller is responsible for multi-part reassembly before calling this.
std::optional<AISMessage> decode_payload(const char* payload_str,
                                         int fill_bits);

}  // namespace ais

#endif  // AIS_DECODER_H_
