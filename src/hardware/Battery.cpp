// Battery.cpp -- see Battery.h for behaviour overview.

#include "Battery.h"
#include "Clock.h"

#include <Arduino.h>
#include <stdio.h>

// ---- public lifecycle -----------------------------------------------------

void Battery::begin(lv_obj_t* parent) {
  createArc(parent);
  createOverlay(parent);
  readVoltageNow();  // _initialized is still false here; no auto-show on boot
  _initialized = true;
  refresh();
}

void Battery::update() {
  const bool shouldRead = Clock::tick(_lastReadAt, BATTERY_UPDATE_MS);

  if (shouldRead) {
    readVoltageNow();
    refresh();
  } else if (_isCharging && shouldShow() && Clock::tick(_lastAnimAt, BATTERY_CHARGE_ANIM_MS)) {
    advanceChargingAnimation();
    refresh();
  }

  // Auto-dismiss overlay after 3 s.
  if (_overlayVisible && Clock::elapsed(_overlayShownAt, 3000)) hideOverlay();

  // Keep the gap label above flash arcs every tick.
  if (_gapLabel && !lv_obj_has_flag(_gapLabel, LV_OBJ_FLAG_HIDDEN)) lv_obj_move_foreground(_gapLabel);
}

void Battery::setMode(BatteryMode mode) {
  _mode = mode;
  refresh();
}

bool Battery::shouldShow() const {
  if (_menuOpen) return false;
  if (_mode == BatteryMode::HIDE) return false;
  if (_usbOnly) return false;  // USB power: hide the indicator entirely
  if (!_batteryPresent) return false;
  switch (_mode) {
    case BatteryMode::SHOW:
      return true;
    case BatteryMode::AUTO:
      return _percent <= BATTERY_AUTO_THRESHOLD || _isCharging;
    case BatteryMode::HIDE:
      return false;
  }
  return false;
}

void Battery::setMenuOpen(bool open) {
  if (_menuOpen == open) return;
  _menuOpen = open;
  if (open) hideOverlay();
  refresh();
}

void Battery::forceRefresh() {
  readVoltageNow();
  refresh();
}

// ---- detail overlay -------------------------------------------------------

void Battery::showOverlay() {
  if (!_overlay || _menuOpen) return;
  _overlayShownAt = Clock::now();
  _overlayVisible = true;

  char pctBuf[8];
  if (_usbOnly)
    snprintf(pctBuf, sizeof(pctBuf), "USB");
  else
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", _percent);
  lv_label_set_text(_overlayPct, pctBuf);

  char voltBuf[16];
  snprintf(voltBuf, sizeof(voltBuf), "%.2f V", _lastVolts);
  lv_label_set_text(_overlayVolt, voltBuf);

  lv_label_set_text(_overlayStatus, _usbOnly ? "USB POWER" : (_isCharging ? "Charging" : "On battery"));

  // Tint the circle border and percentage to match battery level.
  const lv_color_t c = _usbOnly ? COLOR_VALUE_GREY : levelColor(_percent);
  lv_obj_set_style_border_color(_overlayCircle, c, 0);
  lv_obj_set_style_text_color(_overlayPct, c, 0);

  lv_obj_remove_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(_overlay);
}

void Battery::hideOverlay() {
  if (!_overlay) return;
  _overlayVisible = false;
  lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

// ---- private --------------------------------------------------------------

void Battery::readVoltageNow() {
  _lastReadAt = Clock::now();
  const float volts = (analogReadMilliVolts(PIN_BATTERY_ADC) / 1000.0f) * BATTERY_DIVIDER_RATIO;
  _lastVolts = volts;

  const bool wasUsbOnly = _usbOnly;
  _usbOnly = volts >= BATTERY_USB_ONLY_MIN_VOLTS;
  _batteryPresent = (volts >= BATTERY_PRESENT_MIN_VOLTS) && !_usbOnly;
  _isCharging = false;  // no real charge-status pin available on this board
  _percent = _batteryPresent ? voltsToPercent(volts) : 0;

  // Auto-reveal the detail overlay when the USB/no-battery heuristic changes.
  // Skipped on the very first read (begin()) so we don't flash on boot.
  if (_initialized && (_usbOnly != wasUsbOnly)) showOverlay();
}

void Battery::advanceChargingAnimation() {
  _animPercent += 25;
  if (_animPercent > 100) _animPercent = 0;
}

int Battery::voltsToPercent(float v) {
  const float range = BATTERY_FULL_VOLTS - BATTERY_EMPTY_VOLTS;
  if (range <= 0.0f) return 0;
  const float frac = (v - BATTERY_EMPTY_VOLTS) / range;
  if (frac <= 0.0f) return 0;
  if (frac >= 1.0f) return 100;
  return (int)(frac * 100.0f + 0.5f);
}

lv_color_t Battery::levelColor(int pct) {
  if (pct <= 25) return COLOR_MINUS;
  if (pct <= 50) return COLOR_BAT_YELLOW;
  return COLOR_PLUS;
}

// ---- refresh helpers ------------------------------------------------------

void Battery::hideAll() {
  if (_arc) lv_obj_add_flag(_arc, LV_OBJ_FLAG_HIDDEN);
  if (_gapLabel) lv_obj_add_flag(_gapLabel, LV_OBJ_FLAG_HIDDEN);
}

void Battery::showArc(int visualPct, lv_color_t color) {
  lv_arc_set_value(_arc, visualPct);
  lv_obj_set_style_arc_color(_arc, color, LV_PART_INDICATOR);
  lv_obj_remove_flag(_arc, LV_OBJ_FLAG_HIDDEN);
}

void Battery::showPercentLabel(int pct, lv_color_t color) {
  if (!_gapLabel) return;
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  lv_obj_set_style_text_font(_gapLabel, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(_gapLabel, color, 0);
  lv_label_set_text(_gapLabel, buf);
  lv_obj_remove_flag(_gapLabel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(_gapLabel);
}

// Renders the rim arc (and optional percent text) for the current
// battery state. Visibility cases:
//   - shouldShow() == false  -> everything hidden
//   - _usbOnly               -> everything hidden (USB power = no battery)
//   - charging               -> blue arc, animated percent
//   - on battery             -> level-coloured arc, actual percent
void Battery::refresh() {
  if (!_arc) return;

  if (!shouldShow() || _usbOnly) {
    hideAll();
    return;
  }

  const int visualPct = _isCharging ? _animPercent : _percent;
  const lv_color_t color = _isCharging ? lv_color_hex(0x3A86FF) : levelColor(_percent);

  showArc(visualPct, color);

  if (_showPercent)
    showPercentLabel(_percent, color);
  else if (_gapLabel)
    lv_obj_add_flag(_gapLabel, LV_OBJ_FLAG_HIDDEN);
}

// ---- widget builders ------------------------------------------------------

void Battery::createArc(lv_obj_t* parent) {
  _arc = lv_arc_create(parent);
  lv_obj_set_size(_arc, SCREEN_W, SCREEN_H);
  lv_obj_center(_arc);

  // 270° sweep. Gap sits at the bottom between roughly 4:30 and 7:30.
  // LVGL arc: 0° = 3 o'clock, increases clockwise.
  // start=135° ≈ 7:30,  end=45° ≈ 4:30.
  lv_arc_set_bg_angles(_arc, 135, 45);
  lv_arc_set_range(_arc, 0, 100);
  lv_arc_set_value(_arc, 0);

  // Empty track: very dark, squared ends.
  lv_obj_set_style_arc_width(_arc, BATTERY_ARC_WIDTH, LV_PART_MAIN);
  lv_obj_set_style_arc_color(_arc, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(_arc, LV_OPA_100, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(_arc, false, LV_PART_MAIN);

  // Filled indicator: colour driven by refresh().
  lv_obj_set_style_arc_width(_arc, BATTERY_ARC_WIDTH, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(_arc, COLOR_PLUS, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(_arc, false, LV_PART_INDICATOR);

  // Hide the knob.
  lv_obj_set_style_opa(_arc, LV_OPA_TRANSP, LV_PART_KNOB);

  lv_obj_remove_flag(_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(_arc, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(_arc, LV_OBJ_FLAG_HIDDEN);

  // Percentage label sits in the 90° gap at the bottom of the arc.
  // Visible whenever the arc ring is visible; moved to foreground in refresh().
  _gapLabel = lv_label_create(parent);
  lv_obj_remove_style_all(_gapLabel);
  lv_obj_set_style_text_font(_gapLabel, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(_gapLabel, COLOR_PLUS, 0);
  lv_obj_set_style_text_align(_gapLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(_gapLabel, LV_ALIGN_CENTER, 0, 92);
  lv_label_set_text(_gapLabel, "");
  lv_obj_remove_flag(_gapLabel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(_gapLabel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(_gapLabel, LV_OBJ_FLAG_HIDDEN);
}

void Battery::createOverlay(lv_obj_t* parent) {
  // Full-screen dim layer, hidden until showOverlay().
  _overlay = lv_obj_create(parent);
  lv_obj_remove_style_all(_overlay);
  lv_obj_set_size(_overlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(_overlay, 0, 0);
  lv_obj_set_style_bg_color(_overlay, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(_overlay, LV_OPA_80, 0);
  lv_obj_remove_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(_overlay, LV_OBJ_FLAG_CLICKABLE);

  // Detail circle centred on screen.
  _overlayCircle = lv_obj_create(_overlay);
  lv_obj_remove_style_all(_overlayCircle);
  lv_obj_set_size(_overlayCircle, 116, 116);
  lv_obj_center(_overlayCircle);
  lv_obj_set_style_bg_color(_overlayCircle, lv_color_hex(0x181818), 0);
  lv_obj_set_style_bg_opa(_overlayCircle, LV_OPA_100, 0);
  lv_obj_set_style_radius(_overlayCircle, 58, 0);
  lv_obj_set_style_border_width(_overlayCircle, 2, 0);
  lv_obj_set_style_border_color(_overlayCircle, COLOR_PLUS, 0);
  lv_obj_remove_flag(_overlayCircle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(_overlayCircle, LV_OBJ_FLAG_CLICKABLE);

  // Large percentage label.
  _overlayPct = lv_label_create(_overlay);
  lv_obj_remove_style_all(_overlayPct);
  lv_obj_set_style_text_font(_overlayPct, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(_overlayPct, COLOR_PLUS, 0);
  lv_obj_align(_overlayPct, LV_ALIGN_CENTER, 0, -16);
  lv_label_set_text(_overlayPct, "-%");

  // Voltage reading.
  _overlayVolt = lv_label_create(_overlay);
  lv_obj_remove_style_all(_overlayVolt);
  lv_obj_set_style_text_font(_overlayVolt, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(_overlayVolt, COLOR_FG, 0);
  lv_obj_align(_overlayVolt, LV_ALIGN_CENTER, 0, 4);
  lv_label_set_text(_overlayVolt, "-.-- V");

  // Charging / on-battery status.
  _overlayStatus = lv_label_create(_overlay);
  lv_obj_remove_style_all(_overlayStatus);
  lv_obj_set_style_text_font(_overlayStatus, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(_overlayStatus, COLOR_VALUE_GREY, 0);
  lv_obj_align(_overlayStatus, LV_ALIGN_CENTER, 0, 24);
  lv_label_set_text(_overlayStatus, "");

  lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}
