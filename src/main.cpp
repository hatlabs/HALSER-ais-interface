// HALSER AIS Interface Firmware
// Matsutec HA-102 AIS transponder to NMEA 2000 gateway

#include <NMEA2000_esp32.h>

#include <memory>

#include "Wire.h"
#include "elapsedMillis.h"
#include "ais/ais_vdm_parser.h"
#include "matsutec_config.h"
#include "matsutec_ha102_parser.h"
#include "sender/ais_n2k_senders.h"
#include "sender/n2k_senders.h"
#include "sensesp/system/serial_number.h"
#include "sensesp/system/stream_producer.h"
#include "sensesp/transforms/filter.h"
#include "sensesp/transforms/zip.h"
#include "sensesp/ui/status_page_item.h"
#include "sensesp/ui/ui_controls.h"
#include "sensesp_app_builder.h"
#include "sensesp_nmea0183/nmea0183.h"
#include "ssd1306_display.h"
#include "streaming_tcp_client.h"
#include "streaming_tcp_server.h"
#include "ui_config.h"

using namespace sensesp;
using namespace sensesp::nmea0183;
using namespace ais_interface;

// HALSER pin assignments
constexpr int kAISBitRate = 38400;
constexpr gpio_num_t kUART1RxPin = GPIO_NUM_3;
constexpr gpio_num_t kUART1TxPin = GPIO_NUM_2;
constexpr gpio_num_t kCANTxPin = GPIO_NUM_4;
constexpr gpio_num_t kCANRxPin = GPIO_NUM_5;
constexpr int kI2CSDAPin = 6;
constexpr int kI2CSCLPin = 7;
constexpr int kButtonPin = 9;

ObservableValue<int> n2k_rx_counter{0};

elapsedMillis n2k_time_since_rx = 0;

void setup() {
  Serial.setTxTimeoutMs(0);
  SetupLogging(ESP_LOG_DEBUG);

  Wire.setPins(kI2CSDAPin, kI2CSCLPin);
  Wire.begin();

  // SensESP application
  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                    ->set_hostname("ais")
                    ->set_button_pin(kButtonPin)
                    ->enable_ota("thisisfine")
                    ->get_app();

  Serial1.begin(kAISBitRate, SERIAL_8N1, kUART1RxPin, kUART1TxPin);

  auto nmea0183_io_task = std::make_shared<NMEA0183IO>(&Serial1);

  auto mmsi_parser =
      std::make_shared<MatsutecMMSIParser>(nmea0183_io_task->parser_);
  auto static_ship_data_parser =
      std::make_shared<StaticShipDataParser>(nmea0183_io_task->parser_);
  auto voyage_data_parser =
      std::make_shared<VoyageStaticDataParser>(nmea0183_io_task->parser_);

  mmsi_parser->mmsi_.connect_to(std::make_shared<LambdaConsumer<String>>(
      [](String mmsi) { ESP_LOGI("AIS", "MMSI: %s", mmsi.c_str()); }));

  static_ship_data_parser->ship_name_.connect_to(
      std::make_shared<LambdaConsumer<String>>(
          [](String name) { ESP_LOGI("AIS", "Name: %s", name.c_str()); }));

  static_ship_data_parser->callsign_.connect_to(
      std::make_shared<LambdaConsumer<String>>([](String callsign) {
        ESP_LOGI("AIS", "Callsign: %s", callsign.c_str());
      }));

  voyage_data_parser->destination_.connect_to(
      std::make_shared<LambdaConsumer<String>>([](String destination) {
        ESP_LOGI("AIS", "Destination: %s", destination.c_str());
      }));

  voyage_data_parser->draught_.connect_to(
      std::make_shared<LambdaConsumer<float>>(
          [](float draught) { ESP_LOGI("AIS", "Draught: %f", draught); }));

  voyage_data_parser->persons_on_board_.connect_to(
      std::make_shared<LambdaConsumer<int>>(
          [](int persons) { ESP_LOGI("AIS", "Persons: %d", persons); }));

  /////////////////////////////////////////////////////////////////////
  // AIS VDM/VDO sentence parser

  auto ais_vdm_parser =
      std::make_shared<ais::AISVDMSentenceParser>(&nmea0183_io_task->parser_);

  ais_vdm_parser->class_a_position_.connect_to(
      std::make_shared<LambdaConsumer<ais::ClassAPositionReport>>(
          [](ais::ClassAPositionReport report) {
            ESP_LOGI("AIS", "Class A pos: MMSI=%u SOG=%.1f COG=%.1f",
                     report.mmsi, report.sog, report.cog);
          }));

  ais_vdm_parser->class_b_position_.connect_to(
      std::make_shared<LambdaConsumer<ais::ClassBPositionReport>>(
          [](ais::ClassBPositionReport report) {
            ESP_LOGI("AIS", "Class B pos: MMSI=%u SOG=%.1f COG=%.1f",
                     report.mmsi, report.sog, report.cog);
          }));

  ais_vdm_parser->class_a_static_.connect_to(
      std::make_shared<LambdaConsumer<ais::ClassAStaticData>>(
          [](ais::ClassAStaticData data) {
            ESP_LOGI("AIS", "Class A static: MMSI=%u Name=%s Call=%s",
                     data.mmsi, data.name, data.callsign);
          }));

  /////////////////////////////////////////////////////////////////////
  // Config objects

  auto mmsi_config =
      std::make_shared<MMSIConfig>("0", mmsi_parser, &Serial1, "/AIS/MMSI");

  ConfigItem(mmsi_config)
      ->set_title("MMSI")
      ->set_description(
          "The Maritime Mobile Service Identity (MMSI) is a "
          "unique nine-digit number that identifies a vessel to "
          "shore stations and other vessels.")
      ->set_sort_order(1000);

  auto ship_data_config = std::make_shared<ShipDataConfig>(
      "@", "@", 1, 1, 1, 1, static_ship_data_parser, &Serial1,
      "/AIS/Static Ship Data");

  ConfigItem(ship_data_config)
      ->set_title("Static Ship Data")
      ->set_description(
          "The static ship data configuration allows you to "
          "configure the ship's name, callsign, and the distance "
          "from the GNSS antenna to the bow, stern, port, and "
          "starboard.")
      ->set_sort_order(2000);

  time_t arrival_time = 0;

  auto voyage_data_config = std::make_shared<VoyageStaticDataConfig>(
      36, 2.2, 2, "Nowhere", arrival_time, 0, voyage_data_parser, &Serial1,
      "/AIS/Voyage Static Data");

  ConfigItem(voyage_data_config)
      ->set_title("Voyage Static Data")
      ->set_description(
          "The voyage static data configuration allows you to "
          "configure the ship's type, maximum draught, persons on "
          "board, destination, arrival time, and navigational "
          "status.")
      ->set_sort_order(2500);

  auto port_config_ais_tcp_tx = std::make_shared<PortConfig>(
      true, 10110, "/AIS/RX Data TCP Port", "AIS TCP RX Port");

  ConfigItem(port_config_ais_tcp_tx)
      ->set_title("AIS TCP RX Port")
      ->set_description(
          "The port on which the AIS TCP server will listen for "
          "incoming data.")
      ->set_sort_order(3000);

  int ais_tx_port = port_config_ais_tcp_tx->get_port();

  auto host_port_config_sk_nmea0183_tcp_rx = std::make_shared<HostPortConfig>(
      true, "openplotter.local", 10110, "Enable", "Host", "Port",
      "/SignalK/NMEA0183 RX", "Signal K server NMEA 0183 data TCP port");

  ConfigItem(host_port_config_sk_nmea0183_tcp_rx)
      ->set_title("Signal K NMEA 0183 TCP RX")
      ->set_description(
          "The host and port of the Signal K server that will "
          "receive AIS data from this device.")
      ->set_sort_order(5000);

  String sk_hostname = host_port_config_sk_nmea0183_tcp_rx->get_host();
  int sk_n0183_port = host_port_config_sk_nmea0183_tcp_rx->get_port();

  auto ais_tcp_server = std::make_shared<StreamingTCPServer>(
      ais_tx_port, SensESPApp::get()->get_networking(),
      port_config_ais_tcp_tx->get_enabled());

  auto sk_nmea0183_tcp_client = std::make_shared<StreamingTCPClient>(
      sk_hostname, sk_n0183_port, SensESPApp::get()->get_networking(),
      host_port_config_sk_nmea0183_tcp_rx->get_enabled());

  // Wire NMEA 0183 sentences to TCP server and Signal K client
  nmea0183_io_task->sentence_filter_->connect_to(ais_tcp_server);
  nmea0183_io_task->sentence_filter_->connect_to(sk_nmea0183_tcp_client);

  /////////////////////////////////////////////////////////////////////
  // Initialize NMEA 2000 functionality

  auto nmea2000 = std::make_shared<tNMEA2000_esp32>(kCANTxPin, kCANRxPin);

  nmea2000->SetN2kCANSendFrameBufSize(250);
  nmea2000->SetN2kCANReceiveFrameBufSize(250);

  nmea2000->SetProductInformation(
      "20240619",  // Manufacturer's Model serial code (max 32 chars)
      106,         // Manufacturer's product code
      "AIS-N2K",   // Manufacturer's Model ID (max 33 chars)
      "1.1.0",     // Manufacturer's Software version code (max 40 chars)
      "1.0.1"      // Manufacturer's Model version (max 24 chars)
  );

  nmea2000->SetDeviceInformation(
      GetBoardSerialNumber(),  // Unique number
      60,                      // Device function: Navigation
      195,                     // Device class: Sensor Communication Interface
      2047);                   // Manufacturer code

  nmea2000->SetMode(tNMEA2000::N2km_NodeOnly, 74);
  nmea2000->SetMsgHandler([](const tN2kMsg& msg) {
    n2k_rx_counter++;
    n2k_time_since_rx = 0;
  });
  nmea2000->EnableForward(false);

  const unsigned long kAISTransmitMessages[] = {
      129038UL, 129039UL, 129794UL, 129802UL,
      129809UL, 129810UL, 129041UL, 0};
  nmea2000->ExtendTransmitMessages(kAISTransmitMessages);

  nmea2000->Open();

  event_loop()->onRepeat(1, [nmea2000]() { nmea2000->ParseMessages(); });

  /////////////////////////////////////////////////////////////////////
  // AIS → NMEA 2000 senders

  auto class_a_pos_sender =
      std::make_shared<ais::AISClassAPositionN2kSender>(nmea2000.get());
  auto class_b_pos_sender =
      std::make_shared<ais::AISClassBPositionN2kSender>(nmea2000.get());
  auto class_a_static_sender =
      std::make_shared<ais::AISClassAStaticN2kSender>(nmea2000.get());
  auto safety_msg_sender =
      std::make_shared<ais::AISSafetyMessageN2kSender>(nmea2000.get());
  auto class_b_static_sender =
      std::make_shared<ais::AISClassBStaticN2kSender>(nmea2000.get());
  auto aton_sender =
      std::make_shared<ais::AISAtoNN2kSender>(nmea2000.get());

  ais_vdm_parser->class_a_position_.connect_to(class_a_pos_sender);
  ais_vdm_parser->class_b_position_.connect_to(class_b_pos_sender);
  ais_vdm_parser->class_a_static_.connect_to(class_a_static_sender);
  ais_vdm_parser->safety_message_.connect_to(safety_msg_sender);
  ais_vdm_parser->class_b_static_.connect_to(class_b_static_sender);
  ais_vdm_parser->aton_report_.connect_to(aton_sender);

  /////////////////////////////////////////////////////////////////////
  // Configuration elements

  auto enable_n2k_watchdog_config = std::make_shared<CheckboxConfig>(
      false, "Enable NMEA 2000 Watchdog", "/NMEA2000/Enable Watchdog");

  ConfigItem(enable_n2k_watchdog_config)
      ->set_title("NMEA 2000 Watchdog")
      ->set_description(
          "Enable the NMEA 2000 watchdog. If enabled, the device will reboot "
          "after two minutes if no NMEA 2000 messages are received. This "
          "setting requires a device restart to take effect.")
      ->set_sort_order(100);

  if (enable_n2k_watchdog_config->get_value()) {
    event_loop()->onRepeat(1000, [nmea2000]() {
      if (n2k_time_since_rx > 120000) {
        ESP_LOGE("NMEA2000", "No messages received in 2 minutes. Restarting.");
        delay(10);
        ESP.restart();
      }
    });
  }

  auto n2k_rx_ui_output = std::make_shared<StatusPageItem<int>>(
      "NMEA 2000 Received Messages", 0, "NMEA 2000", 300);

  n2k_rx_counter.connect_to(n2k_rx_ui_output);

  /////////////////////////////////////////////////////////////////////
  // Initialize the OLED display

  auto display = std::make_shared<InfoDisplay>(&Wire);

  while (true) {
    loop();
  }
}

void loop() { event_loop()->tick(); }
