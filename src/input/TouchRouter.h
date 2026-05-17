// TouchRouter.h
//
// Encapsulates per-session touch state.
//
// Design notes:
//   - The CST816S emits one event per discrete user action
//     (SINGLE_TAP, SWIPE_*). We trust the panel and process every
//     event; rapid tapping is rate-limited only by TOUCH_COOLDOWN_MS.
//   - The _swallowing flag is for the specific case where the radial
//     menu closes while a finger is still pressed (e.g. centre tap
//     confirms a sub-view). Without it, the CST816S may emit a
//     SINGLE_TAP for that same press right after close, which would
//     bump the life counter. _swallowing drops all events until the
//     finger is confirmed lifted.
//   - Centre-hold tracking is the second piece of state, used by
//     the hold-to-open-menu soft timer.
//   - swallowFirstTouch is a one-shot used right after deep-sleep
//     wake to drop the touch that woke us.

#pragma once

#include "Config.h"
#include "Clock.h"

class TouchRouter {
public:
  struct HoldState {
    bool tracking = false;
    bool menuOpened = false;
    uint32_t startedAt = 0;
    uint32_t lastSeenAt = 0;
    void reset() {
      *this = {};
    }
  };

  void requestSwallowFirstTouch() {
    _swallowFirstTouch = true;
  }

  // Per-tick housekeeping when no new gesture sample is available.
  // Returns the raw finger-down state (1 = down, 0 = lifted, -1 = I2C error).
  //
  // On a CONFIRMED lift (raw == 0): clear the swallow flag and tidy
  // up the centre-hold state if it has timed out. An I2C glitch
  // (raw == -1) keeps state intact.
  int onNoSample();

  // Should the very next touch be swallowed (used right after wake)?
  bool checkSwallowFirstTouch() {
    if (!_swallowFirstTouch) return false;
    _swallowFirstTouch = false;
    return true;
  }

  // True while we're dropping events from a finger that was pressed
  // when the radial menu closed. Cleared on the next confirmed lift.
  bool isSwallowing() const {
    return _swallowing;
  }

  void trackCentreHold(uint32_t now) {
    if (!_hold.tracking) {
      _hold.startedAt = now;
      _hold.tracking = true;
    }
    _hold.lastSeenAt = now;
  }

  void resetHold() {
    _hold.reset();
  }

  bool holdComplete(uint32_t now) const {
    return _hold.tracking && !_hold.menuOpened && (now - _hold.startedAt) >= CENTER_HOLD_MS;
  }

  void markHoldOpenedMenu() {
    _hold.menuOpened = true;
  }

  uint32_t lastActionAt() const {
    return _lastActionAt;
  }
  void recordAction() {
    _lastActionAt = Clock::now();
  }

  bool cooldownExpired() const {
    return Clock::elapsed(_lastActionAt, TOUCH_COOLDOWN_MS);
  }

  // Drop all subsequent gesture events until the finger is confirmed
  // lifted. Used for radial-menu close touches and swipe gestures, so
  // a held finger cannot retrigger the same action.
  void swallowUntilLift() {
    _swallowing = true;
    _lastActionAt = Clock::now();
  }

private:
  HoldState _hold;
  bool _swallowing = false;
  bool _swallowFirstTouch = false;
  uint32_t _lastActionAt = 0;
};
