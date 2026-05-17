// ResetPending.h
//
// Two related types for the shake -> hold-to-confirm reset gesture:
//
//   ResetPendingOverlay  Full-screen dim + progress arc + reset icon.
//                        Shown while ResetPending is active so the user
//                        can confirm the reset by holding the screen.
//   ResetPending         Thin wrapper over HoldConfirmation: armed by a
//                        shake, advanced by a finger-down hold, fires
//                        "reset" once the hold duration is reached.

#pragma once

#include "Config.h"
#include "HoldConfirmation.h"
#include <lvgl.h>

class ResetPendingOverlay {
public:
  void begin(lv_obj_t* parent);
  void show();
  void hide();
  void setProgress(float p);

private:
  lv_obj_t* _dim = nullptr;
  lv_obj_t* _arc = nullptr;
  lv_obj_t* _icon = nullptr;
};

struct ResetPending {
  static constexpr uint32_t HOLD_MS = RESET_HOLD_MS;
  static constexpr uint32_t TIMEOUT_MS = 4000;

  HoldConfirmation state;

  // Convenience accessors so call sites don't have to reach through `state`.
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
