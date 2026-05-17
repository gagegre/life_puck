// Hardware.cpp
//
// Definitions for the singletons declared in Hardware.h plus the
// raw-I2C touch read and the two LVGL bridge callbacks.

#include "Hardware.h"
#include "Config.h"

#include <Wire.h>

namespace Hardware {

TFT_eSPI tft;
CST816S touch(PIN_TOUCH_SDA, PIN_TOUCH_SCL, PIN_TOUCH_RST, PIN_TOUCH_INT);
lv_display_t* lvDisplay = nullptr;

uint8_t drawBuffer[DRAW_BUFFER_BYTES] __attribute__((aligned(4)));

void displayFlush(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
  const uint32_t w = area->x2 - area->x1 + 1;
  const uint32_t h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t*)px, w * h, true);
  tft.endWrite();
  lv_display_flush_ready(disp);
}

void lvTickTask(void*) {
  lv_tick_inc(LVGL_TICK_INTERVAL_MS);
}

int readTouchFingerDownRaw() {
  Wire.beginTransmission(CST816S_I2C_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return -1;
  const uint8_t n = Wire.requestFrom((uint8_t)CST816S_I2C_ADDR, (uint8_t)1);
  if (n != 1 || Wire.available() < 1) return -1;
  return (Wire.read() & 0x0F) > 0 ? 1 : 0;
}

}  // namespace Hardware
