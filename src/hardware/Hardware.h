// Hardware.h
//
// Hardware singletons and low-level glue that doesn't belong in any
// feature class: the TFT driver, the CST816S touch panel, the LVGL
// display handle, the draw buffer, and a couple of raw-I2C helpers.
//
// Each feature module (Battery, LifeCounter, RadialMenu, ...) is
// blissfully unaware of this file. Only the App layer and setup()
// reach in here for wiring.

#pragma once

#include <TFT_eSPI.h>
#include <CST816S.h>
#include <lvgl.h>

#include "Config.h"

namespace Hardware {

// ---- Singletons -----------------------------------------------------------
//
// TFT driver, capacitive touch panel, and LVGL display handle. Defined in
// Hardware.cpp so other translation units can `extern`-import them.

extern TFT_eSPI tft;
extern CST816S touch;
extern lv_display_t* lvDisplay;

// Half-screen draw buffer. The radial overlay flushes in two tiles
// instead of twelve, so it paints near-instantly. ~57.6 KB SRAM, well
// within the ESP32-S3 budget. The size is exposed via the array bound
// so callers can use `sizeof(Hardware::drawBuffer)` directly.
inline constexpr size_t DRAW_BUFFER_BYTES = SCREEN_W * (SCREEN_H / 2) * 2;
extern uint8_t drawBuffer[DRAW_BUFFER_BYTES];

// ---- LVGL bridge callbacks ------------------------------------------------

void displayFlush(lv_display_t* disp, const lv_area_t* area, uint8_t* px);
void lvTickTask(void* arg);

// ---- Direct touch-register read -------------------------------------------
//
// touch.available() is event-like and can return false while a finger
// is still physically pressed. Reading CST816S register 0x02 directly
// gives the true panel state and is what keeps the radial menu from
// flickering closed between gesture samples.
//
// readTouchFingerCountRaw() returns the raw finger count (0, 1, 2, ...)
// or -1 on I2C error. readTouchFingerDownRaw() is the one-bit
// convenience: 1 if any finger is on the panel, 0 if not, -1 on error.
// Both share the same I2C read internally, so cost is one transaction.
int readTouchFingerCountRaw();
int readTouchFingerDownRaw();

}  // namespace Hardware
