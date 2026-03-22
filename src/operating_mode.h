// Receive-only mode for the Matsutec HA-102 transponder.
//
// The HA-102 doesn't have an explicit "receive only" command. Instead,
// setting the MMSI to 000000000 effectively disables AIS transmission
// while allowing reception to continue. The user's configured MMSI is
// preserved in memory so it can be restored when switching back to
// transmit+receive mode.

#ifndef OPERATING_MODE_H_
#define OPERATING_MODE_H_

#include <cstdint>
#include <cstring>
#include <optional>

namespace ais_interface {

enum class OperatingMode : uint8_t {
  kTransmitReceive = 0,
  kReceiveOnly = 1,
};

// Framework-agnostic operating mode state machine.
// Determines what MMSI to send to the transponder based on mode transitions.
class OperatingModeLogic {
 public:
  OperatingModeLogic() = default;

  // Set the user's configured MMSI (from MMSIConfig).
  void set_user_mmsi(const char* mmsi) {
    strncpy(user_mmsi_, mmsi, sizeof(user_mmsi_) - 1);
    user_mmsi_[sizeof(user_mmsi_) - 1] = '\0';
  }

  const char* get_user_mmsi() const { return user_mmsi_; }

  OperatingMode get_mode() const { return mode_; }

  // Transition to a new mode.
  // Returns the MMSI that should be sent to the transponder,
  // or nullopt if no transponder command is needed.
  std::optional<const char*> set_mode(OperatingMode new_mode) {
    if (new_mode == mode_) return std::nullopt;

    mode_ = new_mode;

    if (new_mode == OperatingMode::kReceiveOnly) {
      return kZeroMMSI;
    } else {
      return user_mmsi_;
    }
  }

  // Returns the MMSI that should be sent to the transponder
  // for the current mode. Used at startup to ensure the transponder
  // state matches the configured mode.
  const char* effective_mmsi() const {
    if (mode_ == OperatingMode::kReceiveOnly) {
      return kZeroMMSI;
    }
    return user_mmsi_;
  }

 private:
  static constexpr const char* kZeroMMSI = "000000000";
  OperatingMode mode_ = OperatingMode::kTransmitReceive;
  char user_mmsi_[10] = "000000000";
};

}  // namespace ais_interface

#endif  // OPERATING_MODE_H_
