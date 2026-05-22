// TouchRouter.cpp
//
// onNoSample() is the only method that needs to reach into the Hardware
// layer; everything else is inline in the header.

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
    // Confirmed lift also re-arms the outside-hold tracker (clears the
    // single-shot `fired` latch). Without this the very next press would
    // be ignored.
    _outsideHold.reset();
  }
  return raw;
}

