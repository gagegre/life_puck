// LifeCounter.cpp -- see LifeCounter.h for behaviour overview.

#include "LifeCounter.h"
#include "FlashManager.h"
#include "Clock.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>

// ---- lifecycle ------------------------------------------------------------

void LifeCounter::begin(lv_obj_t* parent, FlashManager* flash) {
  _parent = parent;
  _flash = flash;

  // ---- main life label ----
  _label = lv_label_create(parent);
  lv_obj_set_style_text_color(_label, COLOR_FG, 0);
  lv_obj_set_style_text_font(_label, &montserrat_124, 0);

  // ---- delta badge pill ----
  _deltaLbl = lv_label_create(parent);
  lv_obj_set_style_text_font(_deltaLbl, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(_deltaLbl, COLOR_FG, 0);
  lv_obj_set_style_bg_opa(_deltaLbl, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(_deltaLbl, COLOR_PLUS, 0);
  lv_obj_set_style_radius(_deltaLbl, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_hor(_deltaLbl, 9, 0);
  lv_obj_set_style_pad_ver(_deltaLbl, 3, 0);
  lv_label_set_text(_deltaLbl, "");
  lv_obj_add_flag(_deltaLbl, LV_OBJ_FLAG_HIDDEN);

  // ---- "OF XY" base-life reveal label ----
  // Built once, reused for every reveal. Kept hidden by default so the
  // counter screen looks identical to the previous build until the user
  // long-holds outside the centre. lv_font_montserrat_48 is the built-in
  // SemiBold variant; its rendered width for "OF 30" lands close to the
  // width of "30" at montserrat_124, matching the spec.
  _baseLbl = lv_label_create(parent);
  lv_obj_set_style_text_font(_baseLbl, BASE_LABEL_FONT, 0);
  lv_obj_set_style_text_color(_baseLbl, COLOR_VALUE_GREY, 0);
  lv_obj_set_style_text_opa(_baseLbl, LV_OPA_COVER, 0);
  lv_label_set_text(_baseLbl, "");
  lv_obj_add_flag(_baseLbl, LV_OBJ_FLAG_HIDDEN);

  refreshLabel();
  repositionLifeLabel();
}

// ---- mutation -------------------------------------------------------------

void LifeCounter::change(int delta) {
  const uint32_t now = Clock::now();
  const int prev = _value;
  const int next = constrain(prev + delta, LIFE_MIN, _baseLife);

  // Hit a wall? Fire the bump animation as visual acknowledgement
  // that the input was received but couldn't act on it.
  if (next == prev) {
    if (delta != 0) startBump();
    return;
  }

  // Bundle logic.
  const bool inWindow = _bundleOpen && ((now - _bundleLastAt) < BUNDLE_MS);
  if (!inWindow) {
    pushUndo(prev);
    _accDelta = 0;
    _bundleOpen = true;
  }

  _value = next;
  _accDelta += (next - prev);
  _bundleLastAt = now;

  refreshLabel();
  showDelta(_accDelta);

  const bool isPlus = (delta > 0);
  const bool isHealing = _countUp ? (delta < 0) : (delta > 0);
  if (_flash) _flash->trigger(isPlus, isHealing);

  if (distanceToDefeat() == 0 && _defeatCb) _defeatCb();
}

bool LifeCounter::undo() {
  if (_undoCount == 0) return false;
  _value = popUndo();
  _bundleOpen = false;
  _accDelta = 0;
  _undoPending = false;
  refreshLabel();
  hideDelta();
  return true;
}

bool LifeCounter::beginUndoPending() {
  if (!canUndo()) return false;
  _undoPending = true;
  if (_deltaLbl) {
    const int restore = peekUndo() - _value;
    char buf[8];
    if (restore >= 0)
      snprintf(buf, sizeof(buf), "+%d", restore);
    else
      snprintf(buf, sizeof(buf), "%d", restore);
    lv_label_set_text(_deltaLbl, buf);
    lv_obj_set_style_bg_color(_deltaLbl, COLOR_MENU_ORANGE, 0);
    lv_obj_remove_flag(_deltaLbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(_label);
    repositionDelta();
  }
  refreshLabel();
  return true;
}

void LifeCounter::clearUndoPending() {
  if (!_undoPending) return;
  _undoPending = false;
  showDelta(_accDelta);
  refreshLabel();
}

void LifeCounter::setValue(int v) {
  _value = constrain(v, LIFE_MIN, _baseLife);
  _bundleOpen = false;
  _accDelta = 0;
  clearUndoHistory();
  refreshLabel();
}

void LifeCounter::setBaseLife(int base) {
  _baseLife = constrain(base, 1, LIFE_MAX);
  if (_value > _baseLife) _value = _baseLife;
  refreshLabel();
  refreshBaseLabelText();
}

void LifeCounter::setCountUp(bool up) {
  _countUp = up;
  refreshLabel();
}

void LifeCounter::reset(bool countUpMode) {
  _countUp = countUpMode;
  _bundleOpen = false;
  _accDelta = 0;
  _undoPending = false;
  clearUndoHistory();
  hideDelta();
  // Reset is a global game event; if OF XY happens to be visible, drop it.
  hideBaseReveal();

  _resetFrom = countUpMode ? _baseLife : LIFE_MIN;
  _resetTo = countUpMode ? LIFE_MIN : _baseLife;

  _value = _resetFrom;
  refreshLabel();

  _resetStartAt = Clock::now();
  _resetLastStepAt = _resetStartAt;
  _resetActive = true;
}

void LifeCounter::tapped(int yScreen) {
  // 1P: top half = +1, bottom half = -1.
  change(yScreen < CENTER_Y ? +1 : -1);
}

// ---- OF XY base-life reveal -----------------------------------------------

void LifeCounter::showBaseReveal() {
  if (!_baseLbl || !_label) return;
  _baseRevealActive = true;
  _baseRevealAt = Clock::now();
  // Shift the main counter up so the combined block is vertically centred.
  // Using translate_y keeps LVGL's layout cache cold-free: no re-rasterise.
  lv_obj_set_style_translate_y(_label, Theme::Game::BaseRevealCounterDy, 0);
  refreshBaseLabelText();
  repositionBaseLabel();
  lv_obj_remove_flag(_baseLbl, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(_baseLbl);
  // Keep delta badge above the counter even after the y-shift; cheap.
  if (_deltaLbl && !lv_obj_has_flag(_deltaLbl, LV_OBJ_FLAG_HIDDEN)) repositionDelta();
}

void LifeCounter::hideBaseReveal() {
  if (!_baseRevealActive) return;
  _baseRevealActive = false;
  if (_label) lv_obj_set_style_translate_y(_label, 0, 0);
  if (_baseLbl) lv_obj_add_flag(_baseLbl, LV_OBJ_FLAG_HIDDEN);
}

void LifeCounter::refreshBaseLabelText() {
  if (!_baseLbl) return;
  char buf[12];
  snprintf(buf, sizeof(buf), "OF %d", _baseLife);
  lv_label_set_text(_baseLbl, buf);
}

void LifeCounter::repositionBaseLabel() {
  if (!_baseLbl) return;
  // Anchor to screen centre. The main counter has been translated up
  // (translate_y BaseRevealCounterDy), so the base label sits in the
  // space vacated below it -- the two together form one centred block.
  lv_obj_align(_baseLbl, LV_ALIGN_CENTER, 0, Theme::Game::BaseRevealLabelDy);
}

// ---- layout / visibility --------------------------------------------------

void LifeCounter::useFont(const lv_font_t* f) {
  lv_obj_set_style_text_font(_label, f, 0);
  lv_obj_update_layout(_label);
  repositionDelta();
}

void LifeCounter::setVisible(bool visible) {
  auto setHide = [](lv_obj_t* o, bool hide) {
    if (!o) return;
    if (hide)
      lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  };
  setHide(_label, !visible);
  if (!visible) {
    hideDelta();
    hideBaseReveal();
  }
}

// ---- per-loop tick --------------------------------------------------------

void LifeCounter::update(uint32_t now) {
  // ---- 1. delta bundle ----
  if (_bundleOpen && !_undoPending && (now - _bundleLastAt) >= BUNDLE_MS) {
    _bundleOpen = false;
    hideDelta();
  }

  // ---- 2. rejected-input feedback ----
  if (_bumpActive) {
    const uint32_t elapsed = now - _bumpStartAt;
    if (elapsed >= LIFE_BUMP_MS) {
      _bumpActive = false;
      lv_obj_set_style_text_opa(_label, LV_OPA_COVER, 0);
      lv_obj_set_style_translate_x(_label, 0, 0);
      refreshLabel();
    } else {
      const float t = (float)elapsed / (float)LIFE_BUMP_MS;
      const float decay = 1.0f - t;
      const float angle = t * 2.0f * 2.0f * PI;
      const int dx = (int)lroundf(sinf(angle) * LIFE_BUMP_SHAKE_AMP * decay);

      lv_obj_set_style_translate_x(_label, dx, 0);

      const uint32_t half = LIFE_BUMP_MS / 2;
      const uint32_t safeHalf = half ? half : 1;
      const uint32_t dt = elapsed < half ? elapsed : (LIFE_BUMP_MS - elapsed);
      const uint8_t dip = 60;
      const uint8_t opa = LV_OPA_COVER - (uint8_t)((dip * dt) / safeHalf);
      lv_obj_set_style_text_opa(_label, opa, 0);

      lv_obj_set_style_text_color(_label, Theme::Game::BumpFeedback, 0);
    }
  }

  // ---- 3. reset celebration ----
  if (_resetActive) {
    if (now - _resetLastStepAt >= LIFE_RESET_STEP_MS) {
      _resetLastStepAt = now;

      if (_resetTo > _value) {
        _value++;
      } else if (_resetTo < _value) {
        _value--;
      }

      refreshLabel();

      if (_value == _resetTo) {
        _resetActive = false;
      }
    }
    return;
  }

  // ---- 4. low-HP pulse ----
  updatePulse(now);

  // ---- 5. OF XY auto-hide ----
  if (_baseRevealActive && (now - _baseRevealAt) >= BASE_REVEAL_TIMEOUT_MS) {
    hideBaseReveal();
  }
}

// ---- private --------------------------------------------------------------

static int digitsOf(int v) {
  if (v <= 0) return 1;
  int n = 0;
  while (v > 0) {
    v /= 10;
    n++;
  }
  return n;
}

void LifeCounter::repositionLifeLabel() {
  if (!_label) return;
  lv_obj_align(_label, LV_ALIGN_CENTER, 0, 0);
}

// Anchor the delta badge above the counter. Uses align_to so the badge
// follows whatever translate_y the counter currently has (e.g. during the
// OF XY reveal).
void LifeCounter::repositionDelta() {
  if (!_deltaLbl || !_label) return;
  if (lv_obj_has_flag(_deltaLbl, LV_OBJ_FLAG_HIDDEN)) return;

  lv_obj_update_layout(_deltaLbl);
  lv_obj_align_to(_deltaLbl, _label, LV_ALIGN_OUT_TOP_MID, 0, -2);
}

void LifeCounter::showDelta(int accDelta) {
  if (!_deltaLbl) return;
  char buf[8];
  if (accDelta > 0)
    snprintf(buf, sizeof(buf), "+%d", accDelta);
  else
    snprintf(buf, sizeof(buf), "%d", accDelta);
  lv_label_set_text(_deltaLbl, buf);
  const bool isHealing = _countUp ? (accDelta < 0) : (accDelta > 0);
  lv_obj_set_style_bg_color(_deltaLbl, isHealing ? COLOR_PLUS : COLOR_MINUS, 0);
  lv_obj_remove_flag(_deltaLbl, LV_OBJ_FLAG_HIDDEN);
  repositionDelta();
}

void LifeCounter::hideDelta() {
  if (_deltaLbl) lv_obj_add_flag(_deltaLbl, LV_OBJ_FLAG_HIDDEN);
}

void LifeCounter::startBump() {
  _bumpActive = true;
  _bumpStartAt = Clock::now();
}

lv_color_t LifeCounter::zoneColor(int distance) const {
  if (distance == 0) return COLOR_MINUS;
  if (distance <= LIFE_ZONE_RED_MAX) return COLOR_MINUS;
  if (distance <= LIFE_ZONE_YELLOW_MAX) return COLOR_BAT_YELLOW;
  return COLOR_FG;
}

void LifeCounter::updatePulse(uint32_t now) {
  const int d = distanceToDefeat();
  const bool inRedZone = d > 0 && d <= LIFE_ZONE_RED_MAX && !_undoPending && !_bumpActive && !_resetActive;

  if (!inRedZone) {
    if (_pulsing) {
      lv_obj_set_style_text_opa(_label, LV_OPA_COVER, 0);
      _pulsing = false;
    }
    return;
  }
  _pulsing = true;

  const uint32_t phase = now % LIFE_PULSE_PERIOD_MS;
  const uint32_t half = LIFE_PULSE_PERIOD_MS / 2;
  const uint32_t t = phase < half ? phase : (LIFE_PULSE_PERIOD_MS - phase);
  const uint8_t opa = LIFE_PULSE_OPA_MIN + (uint8_t)(((LV_OPA_COVER - LIFE_PULSE_OPA_MIN) * t) / half);
  lv_obj_set_style_text_opa(_label, opa, 0);
}

void LifeCounter::refreshLabel() {
  if (!_label) return;

  const int distance = distanceToDefeat();

  lv_color_t mainColor = _undoPending ? COLOR_VALUE_GREY : zoneColor(distance);
  lv_obj_set_style_text_color(_label, mainColor, 0);
  lv_label_set_text_fmt(_label, "%d", _value);

  // Only re-run the layout when the digit count actually changed. For
  // rapid tapping within the same digit count this skips an expensive
  // layout walk on every tap, which is the main cause of slowdowns under
  // sustained input on the ESP32-S3.
  const int digits = digitsOf(_value);
  if (digits != _lastDigitCount) {
    _lastDigitCount = digits;
    lv_obj_update_layout(_label);
    repositionLifeLabel();
    repositionDelta();
  }
}

// ---- undo history ---------------------------------------------------------

void LifeCounter::pushUndo(int valueBefore) {
  _undoBefore[_undoHead] = valueBefore;
  _undoHead = (_undoHead + 1) % UNDO_HISTORY_DEPTH;
  if (_undoCount < UNDO_HISTORY_DEPTH) _undoCount++;
}

int LifeCounter::popUndo() {
  _undoHead = (_undoHead + UNDO_HISTORY_DEPTH - 1) % UNDO_HISTORY_DEPTH;
  _undoCount--;
  return _undoBefore[_undoHead];
}

int LifeCounter::peekUndo() const {
  const uint8_t idx = (_undoHead + UNDO_HISTORY_DEPTH - 1) % UNDO_HISTORY_DEPTH;
  return _undoBefore[idx];
}

void LifeCounter::clearUndoHistory() {
  _undoHead = 0;
  _undoCount = 0;
}
