// TouchRouter.cpp
//
// Just the one method body that needs Hardware (raw I2C read).

#include "TouchRouter.h"
#include "Hardware.h"

int TouchRouter::onNoSample() {
  const int raw = Hardware::readTouchFingerDownRaw();
  if (raw == 0) {
    _swallowing = false;
    if (_hold.tracking && Clock::elapsed(_hold.lastSeenAt, MENU_RELEASE_GRACE_MS)) _hold.reset();
  }
  return raw;
}
