// Backlight.cpp
//
// Only checkTimeout() lives here; everything else is inline in the
// header. The reason this method is out-of-line is purely organisational:
// it is the single place that fires the idle-sleep callback, and
// keeping it in the .cpp keeps the staged-idle chain visible as a
// short, readable function.

#include "Backlight.h"

void Backlight::checkTimeout() {
  const uint32_t idle = Clock::now() - _lastActivityAt;

  if (idle >= DEEP_SLEEP_MS) {
    if (_onIdleSleep) _onIdleSleep();
  } else if (idle >= SCREEN_OFF_MS && !_isOff) {
    off();
  } else if (idle >= SCREEN_PRE_OFF_MS && !_isOff && !_isPreOff) {
    preOff();
  } else if (idle >= DIM_TIMEOUT_MS && !_isDimmed && !_isOff) {
    dim();
  }
}
