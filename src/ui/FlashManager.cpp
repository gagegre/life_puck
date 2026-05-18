// FlashManager.cpp

#include "FlashManager.h"
#include "Clock.h"

void FlashManager::begin(lv_obj_t* parent) {
  // 1P half-circle arcs, rotated to the right/left rim to match the
  // unified left/right tap axis.
  //   Right half  (+ tap): sweeps 12 o'clock (270 deg) clockwise
  //                        through 3 o'clock to 6 o'clock (90 deg).
  //                        LVGL handles end < start as a wrap-around
  //                        sweep through 0/360.
  //   Left half   (- tap): sweeps 6 o'clock (90 deg) clockwise
  //                        through 9 o'clock to 12 o'clock (270 deg).
  makeArc(_arc1PRight, COLOR_PLUS, 270, 90, parent, 48);
  makeArc(_arc1PLeft, COLOR_MINUS, 90, 270, parent, 48);

  // 2P across: one arc per screen quadrant, each clipped to its quadrant
  // so the flash only paints inside that corner. The angle ranges are
  // LVGL convention (0 deg = 3 o'clock, growing clockwise).
  //   TL = upper-left  quadrant : arc 180-270, clip top-left rect
  //   TR = upper-right quadrant : arc 270-360, clip top-right rect
  //   BL = lower-left  quadrant : arc  90-180, clip bottom-left rect
  //   BR = lower-right quadrant : arc   0- 90, clip bottom-right rect
  makeClippedArc(_arcTL, COLOR_PLUS, 180, 270, parent, 34, 0, 0, CENTER_X, CENTER_Y);
  makeClippedArc(_arcTR, COLOR_MINUS, 270, 360, parent, 34, CENTER_X, 0, CENTER_X, CENTER_Y);
  makeClippedArc(_arcBL, COLOR_MINUS, 90, 180, parent, 34, 0, CENTER_Y, CENTER_X, CENTER_Y);
  makeClippedArc(_arcBR, COLOR_PLUS, 0, 90, parent, 34, CENTER_X, CENTER_Y, CENTER_X, CENTER_Y);
}

void FlashManager::trigger(bool isPlus, bool isHealing, bool isP2, bool twoPlayerMode) {
  const lv_color_t color = isHealing ? COLOR_PLUS : COLOR_MINUS;

  Flash* flash = nullptr;
  lv_obj_t* arc = nullptr;
  if (!twoPlayerMode) {
    // 1P: + tap = right rim, - tap = left rim.
    flash = isPlus ? &_f1PRight : &_f1PLeft;
    arc = isPlus ? _arc1PRight : _arc1PLeft;
  } else if (isP2) {
    // P2 sits at the top of the screen (rotated). Their + tap is screen
    // top-LEFT (their right hand), - tap is screen top-RIGHT.
    flash = isPlus ? &_fTL : &_fTR;
    arc = isPlus ? _arcTL : _arcTR;
  } else {
    // P1 sits at the bottom. + tap is screen bottom-RIGHT, - tap is
    // screen bottom-LEFT.
    flash = isPlus ? &_fBR : &_fBL;
    arc = isPlus ? _arcBR : _arcBL;
  }
  lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
  startFlash(*flash, arc);
}

void FlashManager::update() {
  const uint32_t now = Clock::now();
  fade(_f1PRight, _arc1PRight, now);
  fade(_f1PLeft, _arc1PLeft, now);
  fade(_fTL, _arcTL, now);
  fade(_fTR, _arcTR, now);
  fade(_fBL, _arcBL, now);
  fade(_fBR, _arcBR, now);
}

void FlashManager::setContracted(bool contracted) {
  if (_contracted == contracted) return;
  _contracted = contracted;

  if (!_arc1PRight) return;  // not yet initialised

  // When the battery ring is visible it occupies the outermost
  // BATTERY_ARC_WIDTH pixels of the rim. Inset the flash arcs by
  // that same amount so they don't paint over it.
  const int pad = contracted ? BATTERY_ARC_WIDTH : 0;

  lv_obj_set_style_pad_all(_arc1PRight, pad, 0);
  lv_obj_set_style_pad_all(_arc1PLeft, pad, 0);
  lv_obj_set_style_pad_all(_arcTL, pad, 0);
  lv_obj_set_style_pad_all(_arcTR, pad, 0);
  lv_obj_set_style_pad_all(_arcBL, pad, 0);
  lv_obj_set_style_pad_all(_arcBR, pad, 0);
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

lv_obj_t* FlashManager::makeClip(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* clip = lv_obj_create(parent);
  lv_obj_remove_style_all(clip);
  lv_obj_set_pos(clip, x, y);
  lv_obj_set_size(clip, w, h);
  lv_obj_remove_flag(clip, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(clip, LV_OBJ_FLAG_SCROLLABLE);
  return clip;
}

void FlashManager::makeClippedArc(lv_obj_t*& arc,
                                  lv_color_t color,
                                  int s,
                                  int e,
                                  lv_obj_t* parent,
                                  int width,
                                  int clipX,
                                  int clipY,
                                  int clipW,
                                  int clipH) {
  lv_obj_t* clip = makeClip(parent, clipX, clipY, clipW, clipH);
  arc = lv_arc_create(clip);
  lv_obj_set_size(arc, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(arc, -clipX, -clipY);
  styleArc(arc, color, s, e, width);
  lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
}

void FlashManager::startFlash(Flash& f, lv_obj_t* arc) {
  f = {true, Clock::now()};
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
  const lv_opa_t opa = (lv_opa_t)((uint32_t)LV_OPA_COVER * (FLASH_MS - elapsed) / FLASH_MS);
  lv_obj_set_style_arc_opa(arc, opa, LV_PART_INDICATOR);
}
