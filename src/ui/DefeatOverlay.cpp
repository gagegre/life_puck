// DefeatOverlay.cpp
//
// Implementation of DefeatOverlay.

#include "DefeatOverlay.h"
#include "Clock.h"

void DefeatOverlay::begin(lv_obj_t* parent, lv_obj_t* shakeTarget) {
  _parent = parent;
  _shakeTarget = shakeTarget;

  // ---- affected-area dim ----
  _dim = lv_obj_create(parent);
  lv_obj_remove_style_all(_dim);
  lv_obj_set_size(_dim, SCREEN_W, SCREEN_H);
  lv_obj_center(_dim);
  lv_obj_set_style_bg_color(_dim, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(_dim, Theme::Defeat::DimOpa, 0);
  lv_obj_remove_flag(_dim, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(_dim, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(_dim, LV_OBJ_FLAG_HIDDEN);

  // ---- outside-in pulse layers ----
  buildPulseLayer(_pulseOuter);
  buildPulseLayer(_pulseMid);
  buildPulseLayer(_pulseInner);

  // ---- main offline panel ----
  _panel = lv_obj_create(parent);
  lv_obj_remove_style_all(_panel);
  lv_obj_set_style_bg_color(_panel, COLOR_MINUS, 0);
  lv_obj_set_style_bg_opa(_panel, Theme::Defeat::PanelBgOpa, 0);
  lv_obj_set_style_border_width(_panel, 2, 0);
  lv_obj_set_style_border_color(_panel, COLOR_MINUS, 0);
  lv_obj_set_style_border_opa(_panel, LV_OPA_COVER, 0);
  lv_obj_remove_flag(_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(_panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(_panel, LV_OBJ_FLAG_HIDDEN);

  // ---- centered combined title ----
  // The player marker and BASE LOST are one centered text block.
  // A second 1 px offset copy gives the default LVGL font a bold,
  // heavier look without needing an additional custom font asset.
  _titleShadow = lv_label_create(parent);
  configureTitleLabel(_titleShadow);

  _title = lv_label_create(parent);
  configureTitleLabel(_title);

  setTitleText(UiText::BASE_LOST);
}

void DefeatOverlay::show(int player, bool twoPlayer) {
  _active = true;
  _twoPlayer = twoPlayer;
  _startedAt = Clock::now();
  _player = player;

  _twoPlayer ? setTitleText((_player == 0) ? UiText::BASE_LOST_P1 : UiText::BASE_LOST_P2)
             : setTitleText(UiText::BASE_LOST);

  layoutForPlayer();

  lv_obj_remove_flag(_dim, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_pulseOuter, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_pulseMid, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_pulseInner, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_titleShadow, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_title, LV_OBJ_FLAG_HIDDEN);

  // Keep overlay above life labels/badges, same modal priority as ModeToast.
  lv_obj_move_foreground(_dim);
  lv_obj_move_foreground(_pulseOuter);
  lv_obj_move_foreground(_pulseMid);
  lv_obj_move_foreground(_pulseInner);
  lv_obj_move_foreground(_panel);
  lv_obj_move_foreground(_titleShadow);
  lv_obj_move_foreground(_title);

  update(Clock::now());
}

void DefeatOverlay::cancel() {
  if (!_active) return;
  _active = false;
  lv_obj_add_flag(_dim, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_pulseOuter, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_pulseMid, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_pulseInner, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_titleShadow, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_title, LV_OBJ_FLAG_HIDDEN);
  if (_shakeTarget) {
    lv_obj_set_style_translate_x(_shakeTarget, 0, 0);
    lv_obj_set_style_translate_y(_shakeTarget, 0, 0);
  }
}

void DefeatOverlay::update(uint32_t now) {
  if (!_active) return;
  const uint32_t elapsed = now - _startedAt;
  updatePulse(elapsed);
}

void DefeatOverlay::configureTitleLabel(lv_obj_t* lbl) {
  lv_obj_set_width(lbl, SCREEN_W - 36);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl, COLOR_MINUS, 0);
  lv_obj_set_style_text_opa(lbl, LV_OPA_COVER, 0);
  lv_obj_set_style_text_line_space(lbl, 8, 0);
  lv_obj_set_style_text_letter_space(lbl, 1, 0);
  lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
}

void DefeatOverlay::setTitleText(const char* text) {
  lv_label_set_text(_titleShadow, text ? text : "");
  lv_label_set_text(_title, text ? text : "");
}

void DefeatOverlay::layoutTitleLabel(lv_obj_t* lbl, int xOffset, int yOffset) {
  lv_obj_set_width(lbl, SCREEN_W - 52);
  lv_obj_align(lbl, LV_ALIGN_CENTER, xOffset, yOffset);
}

void DefeatOverlay::buildPulseLayer(lv_obj_t*& obj) {
  obj = lv_obj_create(_parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_style_bg_color(obj, COLOR_MINUS, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_border_color(obj, COLOR_MINUS, 0);
  lv_obj_set_style_border_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void DefeatOverlay::layoutForPlayer() {
  // The defeated state is intentionally global and centered in both
  // 1P and 2P. The counters are hidden behind it by GameUi, so there
  // is no side-specific stacking or divider clutter.
  _cx = CENTER_X;
  _cy = CENTER_Y;

  lv_obj_set_size(_dim, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(_dim, 0, 0);
  lv_obj_set_style_radius(_dim, LV_RADIUS_CIRCLE, 0);

  layoutPulse(_pulseOuter, SCREEN_W, SCREEN_H, LV_RADIUS_CIRCLE);
  layoutPulse(_pulseMid, Theme::Defeat::PulseMidDiam, Theme::Defeat::PulseMidDiam, LV_RADIUS_CIRCLE);
  layoutPulse(_pulseInner, Theme::Defeat::PulseInnerDiam, Theme::Defeat::PulseInnerDiam, LV_RADIUS_CIRCLE);

  lv_obj_set_size(_panel, Theme::Defeat::PanelDiam, Theme::Defeat::PanelDiam);
  lv_obj_set_style_radius(_panel, LV_RADIUS_CIRCLE, 0);
  lv_obj_align(_panel, LV_ALIGN_CENTER, 0, 0);

  layoutTitleLabel(_titleShadow, 1, Theme::Defeat::TitleOffsetY);
  layoutTitleLabel(_title, 0, Theme::Defeat::TitleOffsetY);
}

void DefeatOverlay::layoutPulse(lv_obj_t* obj, int w, int h, int radius) {
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_radius(obj, radius, 0);
  lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);
}

uint8_t DefeatOverlay::wave(uint32_t elapsed, uint32_t offset, uint8_t maxOpa) const {
  const uint32_t phase = (elapsed + offset) % Theme::Defeat::PulseMs;
  const uint32_t half = Theme::Defeat::PulseMs / 2;
  const uint32_t tri = phase < half ? phase : (Theme::Defeat::PulseMs - phase);
  return (uint8_t)((maxOpa * tri) / half);
}

void DefeatOverlay::applyPulse(lv_obj_t* obj, uint8_t bgOpa, uint8_t borderOpa) {
  lv_obj_set_style_bg_opa(obj, bgOpa, 0);
  lv_obj_set_style_border_opa(obj, borderOpa, 0);
}

void DefeatOverlay::updatePulse(uint32_t elapsed) {
  // Staggered offsets make the red wash appear to breathe from the
  // outside toward the life-number area.
  applyPulse(_pulseOuter, wave(elapsed, 0, Theme::Defeat::PulseBgOuter), wave(elapsed, 0, Theme::Defeat::PulseBorderOuter));
  applyPulse(_pulseMid, wave(elapsed, Theme::Defeat::PulseMs / 5, Theme::Defeat::PulseBgMid), wave(elapsed, Theme::Defeat::PulseMs / 5, Theme::Defeat::PulseBorderMid));
  applyPulse(_pulseInner, wave(elapsed, (Theme::Defeat::PulseMs * 2) / 5, Theme::Defeat::PulseBgInner), wave(elapsed, (Theme::Defeat::PulseMs * 2) / 5, Theme::Defeat::PulseBorderInner));

  // The panel itself also breathes a little, but stays calmer than
  // the edge pulse so "BASE LOST" remains readable.
  const uint8_t panelOpa = Theme::Defeat::PanelBreathBase + wave(elapsed, Theme::Defeat::PulseMs / 3, Theme::Defeat::PanelBreathWave);
  lv_obj_set_style_bg_opa(_panel, panelOpa, 0);
}
