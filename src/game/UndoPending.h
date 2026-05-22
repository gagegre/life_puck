// UndoPending.h
//
// Swipe-to-arm, hold-centre-to-confirm undo (1P only).
//
// Flow:
//   1. Swipe left to arm undo.
//   2. The counter dims and shows the restore delta.
//   3. Hold the centre until the orange confirmation ring fills.
//   4. Release early empties the ring; timeout cancels the armed undo.

#pragma once

#include "Config.h"
#include "HoldConfirmation.h"
#include "OuterRingDwell.h"
#include <lvgl.h>

class UndoPendingOverlay {
public:
  void begin(lv_obj_t* parent);
  void show();
  void hide();
  void setProgress(float p);

private:
  OuterRingDwell _dwell;
};

struct UndoPending {
  static constexpr uint32_t HOLD_MS = RESET_HOLD_MS;
  static constexpr uint32_t TIMEOUT_MS = 4000;

  HoldConfirmation state;

  bool active() const {
    return state.active;
  }
  bool fingerDown() const {
    return state.fingerDown;
  }

  void arm() {
    state.arm();
  }
  void cancel() {
    state.cancel();
  }
  bool timedOut() const {
    return state.timedOut(TIMEOUT_MS);
  }
  void beginHold() {
    state.beginHold(HOLD_MS);
  }
  void releaseHold() {
    state.releaseHold();
  }
  float holdProgress() const {
    return state.holdProgress();
  }
  bool holdComplete() const {
    return state.holdComplete();
  }
};
