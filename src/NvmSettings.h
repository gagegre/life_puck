// NvmSettings.h
//
// Thin wrapper around the ESP Preferences (NVS) library that persists
// user preferences across full power-off. begin() must run before any
// getter/setter is called.
//
// What lives here vs. what doesn't:
//   * NVS (this class)     -> survives full power-off. Brightness,
//                              battery display mode, "show %" toggle,
//                              base-life values.
//   * RTC memory (PersistentState in Config.h) -> survives deep sleep
//                              only. Runtime game state: life totals,
//                              count direction, 2P toggle, touch lock.

#pragma once

#include "Config.h"

#include <Preferences.h>

class NvmSettings {
public:
  void begin() {
    _prefs.begin("lifepuck", false);
  }

  // Backlight PWM level (0..255 — see Config.h for the percent mapping).
  uint8_t getBrightness() const {
    return _prefs.getUChar("bri", BACKLIGHT_DEFAULT_LEVEL);
  }
  void setBrightness(uint8_t level) {
    _prefs.putUChar("bri", level);
  }

  BatteryMode getBatteryMode() const {
    return (BatteryMode)_prefs.getUChar("batmode", (uint8_t)BatteryMode::AUTO);
  }
  void setBatteryMode(BatteryMode m) {
    _prefs.putUChar("batmode", (uint8_t)m);
  }

  bool getBatteryShowPct() const {
    return _prefs.getBool("batpct", false);
  }
  void setBatteryShowPct(bool show) {
    _prefs.putBool("batpct", show);
  }

  int getBaseLife1() const {
    return _prefs.getInt("blife1", 30);
  }
  void setBaseLife1(int v) {
    _prefs.putInt("blife1", v);
  }
  int getBaseLife2() const {
    return _prefs.getInt("blife2", 30);
  }
  void setBaseLife2(int v) {
    _prefs.putInt("blife2", v);
  }

private:
  // mutable so getters can stay const; Preferences::getXxx is not const-correct.
  mutable Preferences _prefs;
};
