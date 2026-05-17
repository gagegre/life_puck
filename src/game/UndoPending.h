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

#include "Clock.h"
#include "Animation.h"
#include "Config.h"
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
  lv_obj_t* _hint = nullptr;
};

struct UndoPending {
  bool active = false;
  bool fingerDown = false;
  int player = 0;  // 0 = P1, 1 = P2
  uint32_t startAt = 0;
  uint32_t holdStartAt = 0;
  TimedAnimation holdAnim;

  static constexpr uint32_t HOLD_MS = RESET_HOLD_MS;
  static constexpr uint32_t TIMEOUT_MS = 4000;

  void begin(int p) {
    active = true;
    fingerDown = false;
    player = p;
    startAt = Clock::now();
    holdStartAt = 0;
    holdAnim.stop();
  }

  void cancel() {
    active = false;
    fingerDown = false;
    holdStartAt = 0;
    holdAnim.stop();
  }

  bool timedOut() const {
    return active && Clock::elapsed(startAt, TIMEOUT_MS);
  }

  void beginHold() {
    fingerDown = true;
    holdStartAt = Clock::now();
    holdAnim.start(HOLD_MS);
  }

  void clearHold() {
    fingerDown = false;
    holdStartAt = 0;
    holdAnim.stop();
  }

  float holdProgress() const {
    if (!fingerDown) return 0.0f;
    return holdAnim.progress();
  }

  bool holdComplete() const {
    return fingerDown && holdAnim.complete();
  }
};
