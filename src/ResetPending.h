// ResetPending.h
//
// Two related types for the shake -> hold-to-confirm reset gesture:
//
//   ResetPendingOverlay  Full-screen dim + progress arc + reset icon.
//                        Shown while ResetPending is active so the user
//                        can confirm the reset by holding the screen.
//   ResetPending         State machine: armed by a shake, advanced by a
//                        finger-down hold, fires "reset" once the hold
//                        duration is reached.

#pragma once

#include "Config.h"
#include "Clock.h"
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
  bool active = false;
  bool fingerDown = false;
  uint32_t startAt = 0;
  uint32_t holdStartAt = 0;

  static constexpr uint32_t HOLD_MS = RESET_HOLD_MS;
  static constexpr uint32_t TIMEOUT_MS = 4000;

  void begin() {
    active = true;
    fingerDown = false;
    startAt = Clock::now();
    holdStartAt = 0;
  }
  void cancel() {
    active = false;
    fingerDown = false;
    holdStartAt = 0;
  }

  bool timedOut() const {
    return active && Clock::elapsed(startAt, TIMEOUT_MS);
  }

  float holdProgress() const {
    if (!fingerDown || holdStartAt == 0) return 0.0f;
    return min(1.0f, (float)(Clock::now() - holdStartAt) / (float)HOLD_MS);
  }
  bool holdComplete() const {
    return fingerDown && holdProgress() >= 1.0f;
  }
};
