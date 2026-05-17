// UndoPending.h
//
// Swipe-to-arm, hold-centre-to-confirm undo.
//
// Flow:
//   1. Swipe left/right on a player's half to arm undo for that player.
//   2. The affected counter dims and shows the restore delta.
//   3. Hold the centre until the orange confirmation ring fills.
//   4. Release early empties the ring; timeout cancels the armed undo.

#pragma once

#include "Config.h"
#include "HoldConfirmation.h"
#include <lvgl.h>

class UndoPendingOverlay {
public:
  void begin(lv_obj_t* parent);
  void show(int player, bool twoPlayerMode);
  void hide();
  void setProgress(float p);

private:
  lv_obj_t* _dim = nullptr;
  lv_obj_t* _arc = nullptr;
  lv_obj_t* _icon = nullptr;
};

struct UndoPending {
  static constexpr uint32_t HOLD_MS = RESET_HOLD_MS;
  static constexpr uint32_t TIMEOUT_MS = 4000;

  HoldConfirmation state;
  int player = 0;  // 0 = P1, 1 = P2

  bool active() const {
    return state.active;
  }
  bool fingerDown() const {
    return state.fingerDown;
  }

  // Arm undo for the given player.
  void arm(int p) {
    player = p;
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
