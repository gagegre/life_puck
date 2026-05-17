// TouchRouter.cpp
//
// onNoSample() and pollTwoFinger() are the two methods that need to reach
// into the Hardware layer; everything else is inline in the header.

#include "TouchRouter.h"
#include "Hardware.h"

int TouchRouter::onNoSample() {
  const int raw = Hardware::readTouchFingerDownRaw();
  if (raw == 0) {
    if (_swallowing) {
      _swallowing = false;
      _swallowNextGesture = true;
      _gestureBlockUntil = Clock::now() + HOLD_RELEASE_GESTURE_BLOCK_MS;
    }
    if (_hold.tracking && Clock::elapsed(_hold.lastSeenAt, MENU_RELEASE_GRACE_MS)) _hold.reset();
  }
  return raw;
}

void TouchRouter::pollTwoFinger(int rawCount) {
  // I2C glitch: leave state intact and try again next tick.
  if (rawCount < 0) return;

  // While we're swallowing (after a hold gesture), do not start a fresh
  // two-finger contact -- the existing physical press is not a new tap.
  if (_swallowing) {
    cancelTwoFinger();
    return;
  }

  const uint32_t now = Clock::now();

  if (rawCount >= 1) {
    if (!_twoFingerContact) {
      _twoFingerContact = true;
      _twoFingerSeen = false;
      _twoFingerArmedAt = 0;
    }
    if (rawCount >= 2 && !_twoFingerSeen) {
      _twoFingerSeen = true;
      _twoFingerArmedAt = now;
    }
    return;
  }

  // rawCount == 0: contact ended. Decide whether the just-released
  // contact qualifies as a two-finger tap.
  if (_twoFingerContact && _twoFingerSeen) {
    const uint32_t held = now - _twoFingerArmedAt;
    if (held >= TWO_FINGER_HOLD_MIN_MS && held <= TWO_FINGER_HOLD_MAX_MS) {
      _twoFingerPending = true;
    }
  }
  _twoFingerContact = false;
  _twoFingerSeen = false;
  _twoFingerArmedAt = 0;
}
