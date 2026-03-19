#ifndef AIS_INTERFACE_SRC_SSD1306_DISPLAY_H_
#define AIS_INTERFACE_SRC_SSD1306_DISPLAY_H_

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <sensesp/system/lambda_consumer.h>

#include "sensesp_base_app.h"

namespace ais_interface {

// OLED display width and height, in pixels
const int kScreenWidth = 128;
const int kScreenHeight = 64;

class InfoDisplay {
 public:
  InfoDisplay(TwoWire* i2c) {
    display_ = new Adafruit_SSD1306(kScreenWidth, kScreenHeight, i2c, -1);
    bool init_successful = display_->begin(SSD1306_SWITCHCAPVCC, 0x3C);
    if (!init_successful) {
      ESP_LOGW("InfoDisplay", "SSD1306 allocation failed");
      return;
    }

    sensesp::event_loop()->onDelay(50, [this]() {
      display_->setRotation(2);
      display_->clearDisplay();
      display_->setTextSize(1);
      display_->setTextColor(SSD1306_WHITE);
      display_->setCursor(0, 0);

      display_->display();

      sensesp::event_loop()->onRepeat(1000, [this]() { update(); });
    });
  }

 private:
  Adafruit_SSD1306* display_;

  void clear_row(int row);
  void print_row(int row, String value);

  void update() {
    char row_buf[40];
    print_row(0, sensesp::SensESPBaseApp::get_hostname());
    print_row(1, WiFi.localIP().toString());
    snprintf(row_buf, sizeof(row_buf), "Uptime: %.0f s", millis() / 1000.0);
    print_row(2, row_buf);
    display_->display();
  }
};

}  // namespace ais_interface

#endif  // AIS_INTERFACE_SRC_SSD1306_DISPLAY_H_
