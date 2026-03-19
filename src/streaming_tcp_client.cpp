#include "streaming_tcp_client.h"

namespace ais_interface {

void ExecuteTCPClientTask(void* this_ptr) {
  StreamingTCPClient* this_ = static_cast<StreamingTCPClient*>(this_ptr);

  this_->execute_client_task();
}

}  // namespace ais_interface
