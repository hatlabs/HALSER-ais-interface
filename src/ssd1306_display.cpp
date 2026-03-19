
#include "ssd1306_display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

namespace ais_interface {

void InfoDisplay::clear_row(int row) {
  display_->fillRect(0, 8 * row, kScreenWidth, 8, 0);
}

void InfoDisplay::print_row(int row, String value) {
  clear_row(row);
  // Cut value to 21 characters
  String cut_value = value;
  if (value.length() > 21) {
    cut_value = value.substring(0, 21);
  }
  display_->setCursor(0, 8 * row);
  display_->printf("%s", cut_value.c_str());
}

}  // namespace ais_interface
