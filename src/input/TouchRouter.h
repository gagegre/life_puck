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

  // Separate hold tracker for the "long-press outside centre = reveal OF XY"
  // gesture. Kept distinct from the centre-hold tracker so the two can never
  // be armed at the same time: this one resets the instant the finger enters
  // the centre dead-zone, and the centre tracker resets when the finger is
  // outside the centre hit circle.
  struct OutsideHoldState {
    bool tracking = false;
    bool fired = false;  // hold already committed; wait for lift before re-arm
    uint32_t startedAt = 0;
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

  // Drop gesture samples that arrive after a hold gesture has already been
  // handled through raw finger tracking. Some CST816S sequences report one or
  // more late SINGLE_TAP samples on release; this prevents that release from
  // changing life.
  void swallowNextGesture() {
    _swallowNextGesture = true;
    _gestureBlockUntil = Clock::now() + HOLD_RELEASE_GESTURE_BLOCK_MS;
    _lastActionAt = Clock::now();
  }

  bool checkSwallowGesture() {
    const uint32_t now = Clock::now();

    if (_swallowing) {
      // Finger is still physically down from a hold gesture. Keep the block
      // window anchored to now so it starts counting from the actual lift,
      // not from when the swallow was first armed.
      _swallowNextGesture = true;
      _gestureBlockUntil = now + HOLD_RELEASE_GESTURE_BLOCK_MS;
      return true;
    }

    if (_swallowNextGesture) {
      // First gesture after lift: arm the fixed 180ms window and consume
      // the one-shot flag. Subsequent gestures within the window are
      // swallowed but do NOT extend it -- letting the window expire
      // naturally so rapid taps after a swipe are not blocked indefinitely.
      _swallowNextGesture = false;
      _gestureBlockUntil = now + HOLD_RELEASE_GESTURE_BLOCK_MS;
      return true;
    }

    if (now < _gestureBlockUntil) {
      return true;
    }

    return false;
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
    // Centre hold and outside hold are mutually exclusive: if the finger is
    // here, the outside-hold tracker can't be valid.
    _outsideHold.reset();
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

  // ---- Outside-centre hold (OF XY reveal) -------------------------------
  // Armed when the finger stays outside the centre dead-zone for OUTSIDE_HOLD_MS.
  // Single-shot: once fired, the tracker waits for the next confirmed lift
  // before it will arm again -- so the same press cannot trigger twice.
  void trackOutsideHold(uint32_t now) {
    if (_outsideHold.fired) return;
    if (!_outsideHold.tracking) {
      _outsideHold.startedAt = now;
      _outsideHold.tracking = true;
    }
  }

  void resetOutsideHold() {
    _outsideHold.tracking = false;
    _outsideHold.startedAt = 0;
    // Note: do NOT clear `fired` here. It stays set until a confirmed lift,
    // which happens in onNoSample().
  }

  bool outsideHoldComplete(uint32_t now) const {
    return _outsideHold.tracking && !_outsideHold.fired &&
           (now - _outsideHold.startedAt) >= OUTSIDE_HOLD_MS;
  }

  void markOutsideHoldFired() {
    _outsideHold.fired = true;
    _outsideHold.tracking = false;
  }

  void recordAction() {
    _lastActionAt = Clock::now();
  }

  bool cooldownExpired() const {
    return Clock::elapsed(_lastActionAt, TOUCH_COOLDOWN_MS);
  }

  // Drop all subsequent gesture events until the finger is confirmed lifted.
  // After lift, keep blocking briefly because the CST816S can emit a delayed
  // SINGLE_TAP for the release.
  void swallowUntilLift() {
    const uint32_t now = Clock::now();
    _swallowing = true;
    _swallowNextGesture = true;
    _gestureBlockUntil = now + HOLD_RELEASE_GESTURE_BLOCK_MS;
    _lastActionAt = now;
  }

private:
  HoldState _hold;
  OutsideHoldState _outsideHold;
  bool _swallowing = false;
  bool _swallowFirstTouch = false;
  bool _swallowNextGesture = false;
  uint32_t _gestureBlockUntil = 0;
  uint32_t _lastActionAt = 0;
};
