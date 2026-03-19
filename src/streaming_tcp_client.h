#ifndef AIS_INTERFACE_SRC_STREAMING_TCP_CLIENT_H_
#define AIS_INTERFACE_SRC_STREAMING_TCP_CLIENT_H_

#include <Arduino.h>
#include <WiFi.h>

#include "buffered_tcp_client.h"
#include "sensesp/net/networking.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/system/task_queue_producer.h"

namespace ais_interface {

void ExecuteTCPClientTask(void* this_ptr);

/**
 * @brief TCP client that is able to receive and transmit continuous data
 * streams.
 */
class StreamingTCPClient : public sensesp::ValueProducer<String>,
                           public sensesp::ValueConsumer<String> {
 public:
  StreamingTCPClient(const String& host, const uint16_t port,
                     std::shared_ptr<sensesp::Networking> networking,
                     bool enabled = true)
      : networking_{networking}, host_{host}, port_{port}, enabled_{enabled} {
    task_event_loop_ = std::make_shared<reactesp::ReactESP>();
    client_ =
        std::make_shared<BufferedTCPClient>(std::make_shared<WiFiClient>());
    tx_queue_producer_ = std::make_shared<sensesp::TaskQueueProducer<String>>(
        "", task_event_loop_, 491);
    rx_queue_producer_ = std::make_shared<sensesp::TaskQueueProducer<String>>(
        "", sensesp::event_loop(), 492);

    if (enabled_) {
      xTaskCreate(ExecuteTCPClientTask, "tcp_client_task", 4096, this, 1, NULL);

      rx_queue_producer_->connect_to(new sensesp::LambdaConsumer<String>(
          [this](const String& str) { this->emit(str); }));
    }
  }

  void set(const String& new_value) override {
    tx_queue_producer_->set(new_value);
  }

 protected:
  std::shared_ptr<sensesp::Networking> networking_;
  const String host_;
  const uint16_t port_;

  std::shared_ptr<BufferedTCPClient> client_;

  std::shared_ptr<sensesp::TaskQueueProducer<String>> tx_queue_producer_;
  std::shared_ptr<sensesp::TaskQueueProducer<String>> rx_queue_producer_;

  sensesp::ObservableValue<String> tx_string_;

  std::shared_ptr<reactesp::ReactESP> task_event_loop_ = nullptr;

  bool enabled_ = true;

  void execute_client_task() {
    this->tx_queue_producer_->connect_to(new sensesp::LambdaConsumer<String>(
        [this](const String& str) { this->tx_string_ = str; }));

    auto send_data = new sensesp::LambdaConsumer<String>([this](String str) {
      if (client_->client_->connected()) {
        client_->client_->write(str.c_str());
      }
    });

    task_event_loop_->onRepeat(2000, [this]() {
      if (client_->client_->connected()) {
        client_->client_->write("\r\n");
      }
    });

    task_event_loop_->onRepeat(100, [this]() {
      if (client_->client_->connected()) {
        client_->client_->flush();
      }
    });

    tx_string_.connect_to(send_data);

    task_event_loop_->onRepeat(1, [this]() {
      if (client_->available() || client_->client_->connected()) {
        String line;
        while (this->client_->read_line(line)) {
          this->rx_queue_producer_->set(line);
        }
      }
    });

    task_event_loop_->onRepeat(1000, [this]() {
      if (!client_->client_->connected()) {
        client_->client_->stop();
        client_->clear_buf();
        debugD("Connecting to %s:%d...", host_.c_str(), port_);
        client_->client_->connect(host_.c_str(), port_);
        debugD("Connected");
      }
    });

    while (true) {
      task_event_loop_->tick();
      delay(1);
    }
  }

  friend void ExecuteTCPClientTask(void* task_args);
};

}  // namespace ais_interface

#endif  // AIS_INTERFACE_SRC_STREAMING_TCP_CLIENT_H_
