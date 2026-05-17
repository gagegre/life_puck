// UndoPending.cpp
//
// Orange confirmation ring used by swipe-to-arm undo. It intentionally
// mirrors ResetPendingOverlay so reset and undo share the same mental model:
// an armed action is only executed after a deliberate centre hold.

#include "UndoPending.h"
#include "Theme.h"

void UndoPendingOverlay::begin(lv_obj_t* parent) {
  _dim = lv_obj_create(parent);
  lv_obj_remove_style_all(_dim);
  lv_obj_set_size(_dim, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(_dim, 0, 0);
  lv_obj_set_style_bg_color(_dim, Theme::Core::Background, 0);
  lv_obj_set_style_bg_opa(_dim, LV_OPA_40, 0);
  lv_obj_remove_flag(_dim, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(_dim, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(_dim, LV_OBJ_FLAG_HIDDEN);

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
  lv_obj_set_style_arc_color(_arc, Theme::Confirm::UndoTrack, LV_PART_MAIN);
  lv_obj_set_style_arc_color(_arc, Theme::Confirm::Undo, LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(_arc, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(_arc, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(_arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(_arc, true, LV_PART_INDICATOR);
  lv_obj_remove_flag(_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(_arc, LV_OBJ_FLAG_HIDDEN);

  _icon = lv_label_create(parent);
  lv_obj_set_style_text_font(_icon, &font_awesome_icons, 0);
  lv_obj_set_style_text_color(_icon, Theme::Confirm::Undo, 0);
  lv_obj_set_style_text_opa(_icon, LV_OPA_COVER, 0);
  lv_label_set_text(_icon, FA_ICON_UNDO);
  lv_obj_align(_icon, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(_icon, LV_OBJ_FLAG_HIDDEN);
}

void UndoPendingOverlay::show(int player, bool twoPlayerMode) {
  (void)player;
  (void)twoPlayerMode;
  lv_arc_set_value(_arc, 0);
  lv_obj_remove_flag(_dim, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_arc, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_icon, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(_dim);
  lv_obj_move_foreground(_arc);
  lv_obj_move_foreground(_icon);
}

void UndoPendingOverlay::hide() {
  lv_obj_add_flag(_dim, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_arc, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_icon, LV_OBJ_FLAG_HIDDEN);
}

void UndoPendingOverlay::setProgress(float p) {
  lv_arc_set_value(_arc, (int)(constrain(p, 0.0f, 1.0f) * 100.0f));
  lv_obj_move_foreground(_arc);
  lv_obj_move_foreground(_icon);
}
