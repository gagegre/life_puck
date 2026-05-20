// OuterRingDwell.cpp

#include "OuterRingDwell.h"
#include "Theme.h"

namespace {

struct Params {
  lv_color_t  arcColor;
  lv_color_t  trackColor;
  uint8_t     arcWidth;
  uint8_t     trackWidth;
  lv_opa_t    arcOpa;
  lv_opa_t    trackOpa;
  lv_opa_t    dimOpa;    // 0 = no dim layer
  int16_t     sizeOffset;
  bool        arcRounded;
  const char* icon;      // null = no icon
};

Params resolveStyle(OuterRingDwell::Style style) {
  switch (style) {
    case OuterRingDwell::Style::Reset:
      return { Theme::Confirm::Reset, Theme::Confirm::ResetTrack,
               10, 10, LV_OPA_COVER, LV_OPA_COVER, LV_OPA_60, 0, true,
               Theme::Confirm::ResetIcon };
    case OuterRingDwell::Style::Undo:
      return { Theme::Confirm::Undo, Theme::Confirm::UndoTrack,
               10, 10, LV_OPA_COVER, LV_OPA_COVER, LV_OPA_40, 0, true,
               Theme::Confirm::UndoIcon };
    case OuterRingDwell::Style::Skip:
      return { Theme::Confirm::SkipArc, Theme::Confirm::SkipTrack,
               3, 1, LV_OPA_80, LV_OPA_30, 0, -14, false, nullptr };
  }
  return {};  // unreachable
}

}  // namespace

void OuterRingDwell::begin(lv_obj_t* parent, Style style) {
  const Params p = resolveStyle(style);

  if (p.dimOpa > 0) {
    _dim = lv_obj_create(parent);
    lv_obj_remove_style_all(_dim);
    lv_obj_set_size(_dim, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_dim, 0, 0);
    lv_obj_set_style_bg_color(_dim, Theme::Core::Background, 0);
    lv_obj_set_style_bg_opa(_dim, p.dimOpa, 0);
    lv_obj_remove_flag(_dim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(_dim, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_dim, LV_OBJ_FLAG_HIDDEN);
  }

  const int16_t sz = SCREEN_W + p.sizeOffset;
  _arc = lv_arc_create(parent);
  lv_obj_set_size(_arc, sz, sz);
  lv_obj_center(_arc);
  lv_arc_set_rotation(_arc, 270);
  lv_arc_set_bg_angles(_arc, 0, 360);
  lv_arc_set_range(_arc, 0, 100);
  lv_arc_set_value(_arc, 0);
  lv_obj_set_style_opa(_arc, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_arc_width(_arc, p.arcWidth, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(_arc, p.trackWidth, LV_PART_MAIN);
  lv_obj_set_style_arc_color(_arc, p.trackColor, LV_PART_MAIN);
  lv_obj_set_style_arc_color(_arc, p.arcColor, LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(_arc, p.trackOpa, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(_arc, p.arcOpa, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(_arc, p.arcRounded, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(_arc, p.arcRounded, LV_PART_INDICATOR);
  lv_obj_remove_flag(_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(_arc, LV_OBJ_FLAG_HIDDEN);

  if (p.icon) {
    _icon = lv_label_create(parent);
    lv_obj_set_style_text_font(_icon, &font_awesome_icons, 0);
    lv_obj_set_style_text_color(_icon, p.arcColor, 0);
    lv_obj_set_style_text_opa(_icon, LV_OPA_COVER, 0);
    lv_label_set_text(_icon, p.icon);
    lv_obj_align(_icon, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(_icon, LV_OBJ_FLAG_HIDDEN);
  }
}

void OuterRingDwell::show() {
  lv_arc_set_value(_arc, 0);
  if (_dim) {
    lv_obj_remove_flag(_dim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_dim);
  }
  lv_obj_remove_flag(_arc, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(_arc);
  if (_icon) {
    lv_obj_remove_flag(_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_icon);
  }
}

void OuterRingDwell::hide() {
  lv_arc_set_value(_arc, 0);
  if (_dim) lv_obj_add_flag(_dim, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_arc, LV_OBJ_FLAG_HIDDEN);
  if (_icon) lv_obj_add_flag(_icon, LV_OBJ_FLAG_HIDDEN);
}

void OuterRingDwell::setProgress(float p) {
  lv_arc_set_value(_arc, (int)(constrain(p, 0.0f, 1.0f) * 100.0f));
  if (lv_obj_has_flag(_arc, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_remove_flag(_arc, LV_OBJ_FLAG_HIDDEN);
    if (_icon) lv_obj_remove_flag(_icon, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_move_foreground(_arc);
  if (_icon) lv_obj_move_foreground(_icon);
}
