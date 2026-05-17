// UndoPending.h
//
// Two-step swipe-to-undo confirmation.
//
// Swipe left/right enters PENDING; the next tap on the player's
// top half confirms (applies undo) or bottom half cancels.
// Any other gesture, shake, or timeout also cancels.

#pragma once

#include "Clock.h"

struct UndoPending {
  bool active = false;
  int player = 0;  // 0 = P1, 1 = P2
  uint32_t startAt = 0;
  static constexpr uint32_t TIMEOUT_MS = 3000;

  void begin(int p) {
    active = true;
    player = p;
    startAt = Clock::now();
  }
  void cancel() {
    active = false;
  }
  bool timedOut() const {
    return active && Clock::elapsed(startAt, TIMEOUT_MS);
  }
};
