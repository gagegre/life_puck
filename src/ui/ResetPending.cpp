// ResetPending.cpp
//
// Implementation of ResetPendingOverlay. ResetPending struct is fully
// inline in the header.

#include "ResetPending.h"

void ResetPendingOverlay::begin(lv_obj_t* parent) {
  // dim layer
  _dim = lv_obj_create(parent);
  lv_obj_remove_style_all(_dim);
  lv_obj_set_size(_dim, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(_dim, 0, 0);
  lv_obj_set_style_bg_color(_dim, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(_dim, LV_OPA_60, 0);
  lv_obj_remove_flag(_dim, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(_dim, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(_dim, LV_OBJ_FLAG_HIDDEN);

  // fill arc
  _arc = lv_arc_create(parent);
  lv_obj_set_size(_arc, SCREEN_W, SCREEN_H);
  lv_obj_center(_arc);
  lv_arc_set_rotation(_arc, 270);
  lv_arc_set_bg_angles(_arc, 0, 360);
  lv_arc_set_range(_arc, 0, 100);
  lv_arc_set_value(_arc, 0);
  lv_obj_set_style_opa(_arc, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_arc_width(_arc, 10, LV_PART_MAIN);
  lv_obj_set_style_arc_width(_arc, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(_arc, lv_color_hex(0x1A0000), LV_PART_MAIN);
  lv_obj_set_style_arc_color(_arc, COLOR_MINUS, LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(_arc, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(_arc, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(_arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(_arc, true, LV_PART_INDICATOR);
  lv_obj_remove_flag(_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(_arc, LV_OBJ_FLAG_HIDDEN);

  // centre icon - screen-level child to avoid style-stripped parent issues
  _icon = lv_label_create(parent);
  lv_obj_set_style_text_font(_icon, &font_awesome_icons, 0);
  lv_obj_set_style_text_color(_icon, COLOR_MINUS, 0);
  lv_obj_set_style_text_opa(_icon, LV_OPA_COVER, 0);
  lv_label_set_text(_icon, FA_ICON_RESET);
  lv_obj_align(_icon, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(_icon, LV_OBJ_FLAG_HIDDEN);
}

void ResetPendingOverlay::show() {
  lv_arc_set_value(_arc, 0);
  lv_obj_remove_flag(_dim, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_arc, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_icon, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(_dim);
  lv_obj_move_foreground(_arc);
  lv_obj_move_foreground(_icon);
}

void ResetPendingOverlay::hide() {
  lv_obj_add_flag(_dim, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_arc, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_icon, LV_OBJ_FLAG_HIDDEN);
}

void ResetPendingOverlay::setProgress(float p) {
  lv_arc_set_value(_arc, (int)(constrain(p, 0.0f, 1.0f) * 100.0f));
  lv_obj_move_foreground(_arc);
  lv_obj_move_foreground(_icon);
}
