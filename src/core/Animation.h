// Animation.h
//
// Tiny, allocation-free animation helpers for short UI effects. The goal is
// not a full tweening framework; it is a shared way to answer the questions
// every overlay/view kept re-implementing:
//   - is this timed animation active?
//   - how far through it are we?
//   - has it completed?

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

class AnimationScheduler {
public:
  void add(TimedAnimation* anim) {
    if (!anim || _count >= MAX_ANIMATIONS) return;
    _items[_count++] = anim;
  }

  void clear() {
    _count = 0;
  }

  void stopAll() {
    for (uint8_t i = 0; i < _count; ++i) {
      if (_items[i]) _items[i]->stop();
    }
  }

private:
  static constexpr uint8_t MAX_ANIMATIONS = 8;
  TimedAnimation* _items[MAX_ANIMATIONS] = {};
  uint8_t _count = 0;
};
