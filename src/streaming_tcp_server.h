#ifndef AIS_INTERFACE_SRC_STREAMING_TCP_SERVER_H_
#define AIS_INTERFACE_SRC_STREAMING_TCP_SERVER_H_

#include <Arduino.h>
#include <WiFi.h>

#include <list>
#include <memory>

#include "buffered_tcp_client.h"
#include "sensesp/net/networking.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/system/valueconsumer.h"

namespace ais_interface {

constexpr size_t kMaxClients = 10;

/**
 * @brief TCP server that is able to receive and transmit continuous data
 * streams.
 *
 */
class StreamingTCPServer : public sensesp::ValueProducer<String>,
                           public sensesp::ValueConsumer<String> {
 public:
  StreamingTCPServer(const uint16_t port, std::shared_ptr<sensesp::Networking> networking,
                     bool enabled = true)
      : networking_{networking}, port_{port}, enabled_{enabled} {
    server_ = std::make_shared<WiFiServer>(port);

    sensesp::event_loop()->onRepeatMicros(100, [this]() {
      this->check_connections();
      this->check_client_input();
    });

    if (enabled_) {
      networking_->connect_to(new sensesp::LambdaConsumer<sensesp::WiFiState>(
          [this](sensesp::WiFiState state) {
            if ((state == sensesp::WiFiState::kWifiConnectedToAP) ||
                (state == sensesp::WiFiState::kWifiAPModeActivated)) {
              debugI("Starting Streaming TCP server on port %d", port_);
              server_->begin();
            }
          }));
    }
  }

  void send_buf(String value) {
    for (auto it = clients_.begin(); it != clients_.end(); it++) {
      if ((*it).client_ != NULL && (*it).client_->connected()) {
        (*it).client_->write(value.c_str());
      }
    }
  }

  void set(const String &new_value) override { send_buf(new_value); }

  void set_enabled(bool enabled) { enabled_ = enabled; }

 protected:
  std::shared_ptr<sensesp::Networking> networking_;
  std::shared_ptr<WiFiServer> server_;
  const uint16_t port_;

  bool enabled_ = true;

  std::list<BufferedTCPClient> clients_;

  void add_client(WiFiClient &client) {
    debugD("New client connected");
    clients_.push_back(
        BufferedTCPClient(WiFiClientPtr(new WiFiClient(client))));
  }

  void stop_client(std::list<BufferedTCPClient>::iterator &it) {
    debugD("Client disconnected");
    (*it).client_->stop();
    it = clients_.erase(it);
  }

  void check_connections() {
    WiFiClient client = server_->available();

    if (client) {
      add_client(client);
    }

    for (auto it = clients_.begin(); it != clients_.end();) {
      if ((*it).client_ != NULL) {
        if (!(*it).client_->connected()) {
          stop_client(it);
        } else {
          ++it;
        }
      } else {
        debugW("Client did not get automatically erased");
        it = clients_.erase(it);
      }
    }
  }

  void check_client_input() {
    for (auto it = clients_.begin(); it != clients_.end(); it++) {
      if ((*it).client_ != NULL && (*it).client_->connected()) {
        String line;
        while ((*it).read_line(line)) {
          this->emit(line);
        }
      }
    }
  }
};

}  // namespace ais_interface

#endif  // AIS_INTERFACE_SRC_STREAMING_TCP_SERVER_H_
