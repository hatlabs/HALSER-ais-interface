#ifndef AIS_INTERFACE_SRC_SENDER_N2K_SENDERS_H_
#define AIS_INTERFACE_SRC_SENDER_N2K_SENDERS_H_

#include <N2kMessages.h>
#include <NMEA2000.h>

#include <tuple>

#include "sensesp/system/expiring_value.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/system/saveable.h"
#include "sensesp/system/serializable.h"
#include "sensesp_base_app.h"

namespace ais_interface {

/**
 * @brief Base class for NMEA 2000 senders.
 *
 */
class N2kSender : public sensesp::FileSystemSaveable,
                  virtual public sensesp::Serializable {
 public:
  N2kSender(String config_path)
      : sensesp::FileSystemSaveable{config_path}, sensesp::Serializable() {}

  virtual void enable() = 0;

  void disable() {
    if (this->sender_reaction_ != nullptr) {
      sensesp::event_loop()->remove(this->sender_reaction_);
      this->sender_reaction_ = nullptr;
    }
  }

 protected:
  reactesp::RepeatReaction* sender_reaction_ = nullptr;
};

}  // namespace ais_interface

#endif  // AIS_INTERFACE_SRC_SENDER_N2K_SENDERS_H_
