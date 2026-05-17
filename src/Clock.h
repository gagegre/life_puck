// Clock.h
//
// Thin facade over millis(). Using Clock::elapsed() / Clock::tick()
// instead of raw `millis() - x >= y` arithmetic keeps timeout conditions
// reading like plain English and makes all timer logic consistent across
// the codebase.
//
// No project-level dependencies; safe to include from any header.

#pragma once

#include <Arduino.h>

namespace Clock {

// Current monotonic-ish timestamp in milliseconds.
inline uint32_t now() {
  return millis();
}

// True once `period` ms have passed since `since`.
inline bool elapsed(uint32_t since, uint32_t period) {
  return (now() - since) >= period;
}

// Edge-trigger helper: returns true (and updates `last`) once per period.
// Typical use: `if (Clock::tick(_lastReadAt, INTERVAL_MS)) doSomething();`
inline bool tick(uint32_t& last, uint32_t period) {
  if ((now() - last) >= period) {
    last = now();
    return true;
  }
  return false;
}

}  // namespace Clock
