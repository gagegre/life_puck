// FlashManager.cpp

#include "FlashManager.h"
#include "Clock.h"

void FlashManager::begin(lv_obj_t* parent) {
  makeArc(_arc1PPlus, COLOR_PLUS, 180, 360, parent, 48);
  makeArc(_arc1PMinus, COLOR_MINUS, 0, 180, parent, 48);
  constexpr int CUT_HALF_H = 36;
  makeClippedArc(_arc2PP1Plus, COLOR_PLUS, 180, 270, parent, 34,
                 0, 0, CENTER_X, CENTER_Y - CUT_HALF_H);
  makeClippedArc(_arc2PP1Minus, COLOR_MINUS, 90, 180, parent, 34,
                 0, CENTER_Y + CUT_HALF_H, CENTER_X, CENTER_Y - CUT_HALF_H);
  makeClippedArc(_arc2PP2Plus, COLOR_PLUS, 0, 90, parent, 34,
                 CENTER_X, CENTER_Y + CUT_HALF_H, CENTER_X, CENTER_Y - CUT_HALF_H);
  makeClippedArc(_arc2PP2Minus, COLOR_MINUS, 270, 360, parent, 34,
                 CENTER_X, 0, CENTER_X, CENTER_Y - CUT_HALF_H);
}

void FlashManager::trigger(bool isTop, bool isHealing, bool isP2, bool twoPlayerMode) {
  const lv_color_t color = isHealing ? COLOR_PLUS : COLOR_MINUS;

  Flash* flash = nullptr;
  lv_obj_t* arc = nullptr;
  if (!twoPlayerMode) {
    flash = isTop ? &_f1PPlus : &_f1PMinus;
    arc = isTop ? _arc1PPlus : _arc1PMinus;
  } else if (!isP2) {
    flash = isTop ? &_f2PP1Plus : &_f2PP1Minus;
    arc = isTop ? _arc2PP1Plus : _arc2PP1Minus;
  } else {
    flash = isTop ? &_f2PP2Plus : &_f2PP2Minus;
    arc = isTop ? _arc2PP2Plus : _arc2PP2Minus;
  }
  lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
  startFlash(*flash, arc);
}

void FlashManager::update() {
  const uint32_t now = Clock::now();
  fade(_f1PPlus, _arc1PPlus, now);
  fade(_f1PMinus, _arc1PMinus, now);
  fade(_f2PP1Plus, _arc2PP1Plus, now);
  fade(_f2PP1Minus, _arc2PP1Minus, now);
  fade(_f2PP2Plus, _arc2PP2Plus, now);
  fade(_f2PP2Minus, _arc2PP2Minus, now);
}

void FlashManager::setContracted(bool contracted) {
  if (_contracted == contracted) return;
  _contracted = contracted;

  if (!_arc1PPlus) return;  // not yet initialised

  // When the battery ring is visible it occupies the outermost
  // BATTERY_ARC_WIDTH pixels of the rim. Inset the flash arcs by
  // that same amount so they don't paint over it.
  const int pad = contracted ? BATTERY_ARC_WIDTH : 0;

  lv_obj_set_style_pad_all(_arc1PPlus, pad, 0);
  lv_obj_set_style_pad_all(_arc1PMinus, pad, 0);
  lv_obj_set_style_pad_all(_arc2PP1Plus, pad, 0);
  lv_obj_set_style_pad_all(_arc2PP1Minus, pad, 0);
  lv_obj_set_style_pad_all(_arc2PP2Plus, pad, 0);
  lv_obj_set_style_pad_all(_arc2PP2Minus, pad, 0);
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

void FlashManager::makeArc(lv_obj_t*& arc, lv_color_t color, int s, int e,
                           lv_obj_t* parent, int width) {
  arc = lv_arc_create(parent);
  lv_obj_set_size(arc, SCREEN_W, SCREEN_H);
  lv_obj_center(arc);
  styleArc(arc, color, s, e, width);
  lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* FlashManager::makeClip(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* clip = lv_obj_create(parent);
  lv_obj_remove_style_all(clip);
  lv_obj_set_pos(clip, x, y);
  lv_obj_set_size(clip, w, h);
  lv_obj_remove_flag(clip, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(clip, LV_OBJ_FLAG_SCROLLABLE);
  return clip;
}

void FlashManager::makeClippedArc(lv_obj_t*& arc, lv_color_t color, int s, int e,
                                  lv_obj_t* parent, int width,
                                  int clipX, int clipY, int clipW, int clipH) {
  lv_obj_t* clip = makeClip(parent, clipX, clipY, clipW, clipH);
  arc = lv_arc_create(clip);
  lv_obj_set_size(arc, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(arc, -clipX, -clipY);
  styleArc(arc, color, s, e, width);
  lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
}

void FlashManager::startFlash(Flash& f, lv_obj_t* arc) {
  f = { true, Clock::now() };
  // Reset to full opacity on every trigger so back-to-back changes
  // don't inherit a partly-faded arc from a previous flash.
  lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_HIDDEN);
}

// Linear opacity fade from full (LV_OPA_COVER) at trigger time down to
// transparent at FLASH_MS, then hide the arc.
void FlashManager::fade(Flash& f, lv_obj_t* arc, uint32_t now) {
  if (!f.active) return;
  const uint32_t elapsed = now - f.at;
  if (elapsed >= FLASH_MS) {
    f.active = false;
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  // Linear ramp: COVER → TRANSP. Use uint32 to avoid overflow in the
  // intermediate multiplication.
  const lv_opa_t opa =
    (lv_opa_t)((uint32_t)LV_OPA_COVER * (FLASH_MS - elapsed) / FLASH_MS);
  lv_obj_set_style_arc_opa(arc, opa, LV_PART_INDICATOR);
}
