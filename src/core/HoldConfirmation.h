// HoldConfirmation.h
//
// Shared "arm + hold-to-confirm" state machine used by ResetPending and
// UndoPending. Both gestures follow the same grammar -- a deliberate
// trigger arms the action, and a centre hold of HOLD_MS confirms it; if
// the user does nothing for TIMEOUT_MS the armed action quietly cancels.
//
// Lifting this out of two near-duplicate structs gives the reset and undo
// flows a single source of truth for the hold-confirm contract.

#pragma once

#include "Animation.h"
#include "Clock.h"

#include <Arduino.h>
#include <stdint.h>

struct HoldConfirmation {
  bool active = false;
  bool fingerDown = false;
  uint32_t startAt = 0;
  TimedAnimation hold;

  // Arm the action. Call this from the gesture that armed it (e.g. a shake
  // for reset, a swipe for undo). The TIMEOUT_MS clock starts here.
  void arm() {
    active = true;
    fingerDown = false;
    startAt = Clock::now();
    hold.stop();
  }

  // Drop the armed state entirely. Used on explicit cancel, on confirmation,
  // and on the TIMEOUT_MS auto-cancel path.
  void cancel() {
    active = false;
    fingerDown = false;
    hold.stop();
  }

  bool timedOut(uint32_t timeoutMs) const {
    return active && Clock::elapsed(startAt, timeoutMs);
  }

  // Finger landed inside the confirmation hit-target -- start filling the
  // ring. holdMs is the duration the user must keep the finger down to
  // commit; the same value drives the progress arc.
  void beginHold(uint32_t holdMs) {
    fingerDown = true;
    hold.start(holdMs);
  }

  // Finger left the hit-target before the ring filled. The arm remains
  // valid -- the user can come back and try again until TIMEOUT_MS.
  void releaseHold() {
    fingerDown = false;
    hold.stop();
  }

  float holdProgress() const {
    return fingerDown ? hold.progress() : 0.0f;
  }

  bool holdComplete() const {
    return fingerDown && hold.complete();
  }
};
