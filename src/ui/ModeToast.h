// ModeToast.h
//
// Short confirmation pulse after mode/settings changes.
//
// Self-contained "modal disc" design:
//   _dim     : full-screen heavy black overlay (~86% opaque) so the
//              life counter and background almost vanish behind it
//   _circle  : 120 px coloured circle that holds the icon AND title
//   _icon    : Font Awesome glyph, upper half of the circle
//   _title   : caption text, lower half of the circle (multi-line OK)
//
// Everything fades together when TOAST_MS elapses. The icon and title
// are positioned relative to screen centre, not the circle, so LVGL
// doesn't get confused by the style-stripped circle as a parent.

#pragma once

#include "Config.h"
#include <lvgl.h>

class ModeToast {
public:
  static constexpr int CIRCLE_DIAM = 120;
  static constexpr int ICON_OFFSET_Y = -22;  // upper half of circle
  static constexpr int TITLE_OFFSET_Y = 22;  // lower half of circle
  static constexpr int TITLE_MAX_W = 96;     // fits inside circle - padding

  void begin(lv_obj_t* parent);

  // Compose "title value" if both are non-empty. The label wraps to
  // two lines automatically if the resulting string is too wide.
  void show(const char* icon, const char* title, const char* value, lv_color_t color, bool coverScreen = false);

  // Two-arg convenience.
  void show(const char* icon, const char* title, lv_color_t color, bool coverScreen = false) {
    show(icon, title, "", color, coverScreen);
  }

  void update();

private:
  static constexpr uint32_t TOAST_MS = 750;

  lv_obj_t* _parent = nullptr;
  lv_obj_t* _dim = nullptr;
  lv_obj_t* _circle = nullptr;
  lv_obj_t* _icon = nullptr;
  lv_obj_t* _title = nullptr;

  uint32_t _shownAt = 0;
  bool _visible = false;
};
