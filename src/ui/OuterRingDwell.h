// OuterRingDwell.h
//
// Reusable full-screen dwell ring widget: dimmed background layer, a
// full-circle progress arc, and an optional centre icon.  No interaction
// logic, the owner calls setProgress(0..1) each tick.
//
// Used by ResetPendingOverlay, UndoPendingOverlay, and StartupIntro's
// hold-to-skip arc.

#pragma once

#include "Config.h"
#include <lvgl.h>

class OuterRingDwell {
public:
  enum class Style { Reset, Undo, Skip };

  void begin(lv_obj_t* parent, Style style);

  // show() resets progress to 0 and makes all layers visible.
  void show();

  // hide() resets progress to 0 and hides all layers.
  void hide();

  // setProgress() updates the arc fill.  If the arc is currently hidden it is
  // made visible (for callers that drive visibility through setProgress alone).
  void setProgress(float p);

private:
  lv_obj_t* _dim  = nullptr;
  lv_obj_t* _arc  = nullptr;
  lv_obj_t* _icon = nullptr;
};
