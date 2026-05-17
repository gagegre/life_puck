// Animation.h
//
// Tiny, allocation-free animation helper for short UI effects. The goal is
// not a full tweening framework; it is a shared way to answer the questions
// every overlay/view kept re-implementing:
//   - is this timed animation active?
//   - how far through it are we?
//   - has it completed?
//
// Used by RadialMenu's dwell ring and the HoldConfirmation helper.

#pragma once

#include "Clock.h"
#include <Arduino.h>
#include <math.h>

class TimedAnimation {
public:
  void start(uint32_t durationMs) {
    _active = true;
    _startAt = Clock::now();
    _durationMs = max<uint32_t>(1, durationMs);
  }

  void stop() {
    _active = false;
    _startAt = 0;
    _durationMs = 1;
  }

  bool active() const {
    return _active;
  }

  uint32_t elapsed(uint32_t now = Clock::now()) const {
    if (!_active) return 0;
    return now - _startAt;
  }

  bool complete(uint32_t now = Clock::now()) const {
    return _active && elapsed(now) >= _durationMs;
  }

  float progress(uint32_t now = Clock::now()) const {
    if (!_active) return 0.0f;
    return constrain((float)elapsed(now) / (float)_durationMs, 0.0f, 1.0f);
  }

  uint8_t progress255(uint32_t now = Clock::now()) const {
    return (uint8_t)lroundf(progress(now) * 255.0f);
  }

private:
  bool _active = false;
  uint32_t _startAt = 0;
  uint32_t _durationMs = 1;
};
