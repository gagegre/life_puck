// Backlight.h
//
// PWM control for the LCD backlight, plus the three-stage idle-timeout
// chain (dim -> pre-off -> off -> deep sleep).
//
// The deep-sleep step is delegated to a callback rather than calling
// PowerManager directly, so this module has no dependency on the rest
// of the application. The .ino wires it up at startup:
//
//   backlight.setIdleSleepCallback(&PowerManager::deepSleep);

#pragma once

#include "Config.h"
#include "Clock.h"

class Backlight {
public:
  using IdleSleepCallback = void (*)();

  // Inject the action to take when DEEP_SLEEP_MS of inactivity elapses.
  void setIdleSleepCallback(IdleSleepCallback cb) {
    _onIdleSleep = cb;
  }

  // ---- mode setters ------------------------------------------------------

  void dim() {
    _isDimmed = true;
    _isPreOff = false;
    writePwm(BACKLIGHT_DIM_LEVEL);
  }

  // "Pre-off" warning state: even dimmer than `dim`, signalling that the
  // screen is about to turn off in a few seconds. A subtle hint so the
  // user can tap to keep the device awake without surprise.
  void preOff() {
    _isDimmed = true;
    _isPreOff = true;
    writePwm(BACKLIGHT_PRE_OFF_LEVEL);
  }

  void off() {
    _isDimmed = false;
    _isPreOff = false;
    _isOff = true;
    writePwm(0);
  }

  void on() {
    _isDimmed = false;
    _isPreOff = false;
    _isOff = false;
    _lastActivityAt = Clock::now();
    writePwm(_level);
  }

  // ---- idle-timeout machinery -------------------------------------------

  void recordActivity() {
    _lastActivityAt = Clock::now();
    if (_isOff || _isDimmed) on();
  }

  // Returns true if the screen was fully off before this call so the
  // caller can swallow the waking touch.
  bool wakeOnTouch() {
    const bool wasOff = _isOff;
    recordActivity();
    return wasOff;
  }

  // Walks the staged idle chain; fires the idle-sleep callback once the
  // longest timeout (DEEP_SLEEP_MS) is reached.
  void checkTimeout();

  // ---- level / percent API -----------------------------------------------

  uint8_t level() const {
    return _level;
  }
  void setLevel(uint8_t value) {
    setLevel(value, true);
  }

  void setLevel(uint8_t value, bool applyNow) {
    _level = value;
    if (applyNow && !_isOff) writePwm(value);
  }

  // Single conversion point for percent <-> level (also see Config.h).
  int asPercent() const {
    return backlightLevelToPercent(_level);
  }
  void setPercent(int pct) {
    setLevel(backlightPercentToLevel(constrain(pct, 0, 100)));
  }

  // ---- introspection -----------------------------------------------------

  bool isOff() const {
    return _isOff;
  }
  bool isDimmed() const {
    return _isDimmed;
  }
  uint32_t lastActivityAt() const {
    return _lastActivityAt;
  }

private:
  uint8_t _level = BACKLIGHT_DEFAULT_LEVEL;
  bool _isOff = false;
  bool _isDimmed = false;
  bool _isPreOff = false;
  uint32_t _lastActivityAt = 0;
  IdleSleepCallback _onIdleSleep = nullptr;

  static void writePwm(uint8_t value) {
    analogWrite(PIN_LCD_BACKLIGHT, value);
  }
};
