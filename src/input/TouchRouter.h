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
//   - Two-finger detection is the third: while any contact is in
//     progress, callers pollTwoFinger() with the raw finger count;
//     if the count rose to >=2 at some point during a contact and
//     no swipe/tap gesture fired, takeTwoFingerTap() returns true on
//     release.
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
      _swallowNextGesture = true;
      _gestureBlockUntil = now + HOLD_RELEASE_GESTURE_BLOCK_MS;
      return true;
    }

    if (_swallowNextGesture || now < _gestureBlockUntil) {
      _swallowNextGesture = false;
      _gestureBlockUntil = now + HOLD_RELEASE_GESTURE_BLOCK_MS;
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

  // ---- Two-finger tap detection ----
  //
  // The two-finger state machine is driven by polling the raw CST816S
  // finger count each loop iteration. Call pollTwoFinger() with the
  // current raw count (-1 if the I2C read failed -- treated as "no info,
  // hold state").
  //
  // Any single-finger gesture event arriving during the contact aborts
  // detection (cancelTwoFinger()). Detection is also cancelled implicitly
  // while the swallow flag is set, while a menu is open, or whenever the
  // caller wants -- e.g. defeat overlay active.
  //
  // takeTwoFingerTap() returns and consumes a completed tap once.

  // Reset the detector. Use when entering a state in which two-finger
  // tap should not fire (radial menu open, defeat overlay, etc.).
  void cancelTwoFinger() {
    _twoFingerSeen = false;
    _twoFingerContact = false;
    _twoFingerArmedAt = 0;
  }

  // Drive the detector. rawCount is the raw value from
  // Hardware::readTouchFingerCountRaw(): 0/1/2/... or -1 on I2C error.
  // Call this once per loop tick.
  void pollTwoFinger(int rawCount);

  // Returns true exactly once when a qualifying two-finger tap has just
  // completed -- a contact during which the raw count rose to >=2 and
  // then dropped to 0 within [TWO_FINGER_HOLD_MIN_MS, TWO_FINGER_HOLD_MAX_MS]
  // without any swipe/tap gesture being seen.
  bool takeTwoFingerTap() {
    if (!_twoFingerPending) return false;
    _twoFingerPending = false;
    return true;
  }

private:
  HoldState _hold;
  bool _swallowing = false;
  bool _swallowFirstTouch = false;
  bool _swallowNextGesture = false;
  uint32_t _gestureBlockUntil = 0;
  uint32_t _lastActionAt = 0;

  // ---- two-finger detection state ----
  // _twoFingerContact: a contact is currently in progress (raw count >= 1).
  // _twoFingerSeen:    during this contact the raw count rose to >= 2.
  // _twoFingerArmedAt: timestamp at which count first rose to >= 2.
  // _twoFingerPending: a completed two-finger tap is waiting to be taken.
  bool _twoFingerContact = false;
  bool _twoFingerSeen = false;
  uint32_t _twoFingerArmedAt = 0;
  bool _twoFingerPending = false;
};
