// FlashManager.cpp

#include "FlashManager.h"
#include "Clock.h"

void FlashManager::begin(lv_obj_t* parent) {
  // 1P half-circle arcs on the top/bottom rim to match the up/down tap axis.
  //   Top half    (+ tap): sweeps 9 o'clock (180 deg) clockwise
  //                        through 12 o'clock to 3 o'clock (0/360 deg).
  //   Bottom half (- tap): sweeps 3 o'clock (0 deg) clockwise
  //                        through 6 o'clock to 9 o'clock (180 deg).
  makeArc(_arcTop, COLOR_PLUS, 180, 0, parent, Theme::Flash::Arc1PWidth);
  makeArc(_arcBot, COLOR_MINUS, 0, 180, parent, Theme::Flash::Arc1PWidth);
}

void FlashManager::trigger(bool isPlus, bool isHealing) {
  const lv_color_t color = isHealing ? COLOR_PLUS : COLOR_MINUS;
  Flash* flash = isPlus ? &_fTop : &_fBot;
  lv_obj_t* arc = isPlus ? _arcTop : _arcBot;
  lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
  startFlash(*flash, arc);
}

void FlashManager::update() {
  const uint32_t now = Clock::now();
  fade(_fTop, _arcTop, now);
  fade(_fBot, _arcBot, now);
}

void FlashManager::setContracted(bool contracted) {
  if (_contracted == contracted) return;
  _contracted = contracted;

  if (!_arcTop) return;

  const int pad = contracted ? BATTERY_ARC_WIDTH : 0;
  lv_obj_set_style_pad_all(_arcTop, pad, 0);
  lv_obj_set_style_pad_all(_arcBot, pad, 0);
}

// ---- static helpers -------------------------------------------------------

void FlashManager::styleArc(lv_obj_t* arc, lv_color_t color, int s, int e, int width) {
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
  lv_arc_set_range(arc, 0, 100);
  lv_arc_set_bg_angles(arc, s, e);
  lv_arc_set_value(arc, 100);
}

void FlashManager::makeArc(lv_obj_t*& arc, lv_color_t color, int s, int e, lv_obj_t* parent, int width) {
  arc = lv_arc_create(parent);
  lv_obj_set_size(arc, SCREEN_W, SCREEN_H);
  lv_obj_center(arc);
  styleArc(arc, color, s, e, width);
  lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
}

void FlashManager::startFlash(Flash& f, lv_obj_t* arc) {
  f = {true, Clock::now()};
  lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_HIDDEN);
}

void FlashManager::fade(Flash& f, lv_obj_t* arc, uint32_t now) {
  if (!f.active) return;
  const uint32_t elapsed = now - f.at;
  if (elapsed >= FLASH_MS) {
    f.active = false;
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  const lv_opa_t opa = (lv_opa_t)((uint32_t)LV_OPA_COVER * (FLASH_MS - elapsed) / FLASH_MS);
  lv_obj_set_style_arc_opa(arc, opa, LV_PART_INDICATOR);
}
