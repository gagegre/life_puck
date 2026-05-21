// LifeCounter.cpp -- see LifeCounter.h for behaviour overview.

#include "LifeCounter.h"
#include "FlashManager.h"
#include "Clock.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>

// ---- lifecycle ------------------------------------------------------------

void LifeCounter::begin(lv_obj_t* parent, FlashManager* flash, bool isP2) {
  _parent = parent;
  _flash = flash;
  _isP2 = isP2;

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

  refreshLabel();
  centerFull();

  // P2 is seated across the table (rotated 180°). Apply the rotation ONCE here
  // with a percentage pivot so LVGL always rotates around each object's own
  // centre regardless of content width. We never touch the pivot again, which
  // eliminates the per-tap lv_obj_set_style_transform_pivot_* calls that
  // marked the label dirty on every change and caused the WDT freeze.
  if (_isP2) {
    lv_obj_set_style_transform_rotation(_label, 1800, 0);
    lv_obj_set_style_transform_pivot_x(_label, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(_label, lv_pct(50), 0);
    lv_obj_set_style_transform_rotation(_deltaLbl, 1800, 0);
    lv_obj_set_style_transform_pivot_x(_deltaLbl, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(_deltaLbl, lv_pct(50), 0);
  }
}

// ---- mutation -------------------------------------------------------------

void LifeCounter::change(int delta, bool twoPlayerMode) {
  const uint32_t now = Clock::now();
  const int prev = _value;
  const int next = constrain(prev + delta, LIFE_MIN, _baseLife);

  // Hit a wall? Fire the bump animation as visual acknowledgement
  // that the input was received but couldn't act on it.
  if (next == prev) {
    if (delta != 0) startBump();
    return;
  }

  // Bundle logic: a new bundle pushes one undo snapshot. Subsequent
  // changes within BUNDLE_MS continue the same bundle and share the
  // snapshot already on the stack.
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

  // Flash arc:
  //   position = the player's "+/-" rim zone (1P: top/bottom half;
  //              2P across: each player's right/left quadrant).
  //              Always follows delta sign.
  //   colour   = mode-dependent meaning: heal = green, damage = red.
  // The two are decoupled so the visual matches both the finger position
  // and the in-game consequence simultaneously.
  const bool isPlus = (delta > 0);
  const bool isHealing = _countUp ? (delta < 0) : (delta > 0);
  if (_flash) _flash->trigger(isPlus, isHealing, _isP2, twoPlayerMode);

  // Defeat detection: distance == 0 means defeat in either mode.
  if (distanceToDefeat() == 0 && _defeatCb) _defeatCb(_isP2 ? 1 : 0);
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

  _resetFrom = countUpMode ? _baseLife : LIFE_MIN;
  _resetTo = countUpMode ? LIFE_MIN : _baseLife;

  // First visible frame must start at the animation start value.
  _value = _resetFrom;
  refreshLabel();

  _resetStartAt = Clock::now();
  _resetLastStepAt = _resetStartAt;
  _resetActive = true;
}

void LifeCounter::tapped(int xScreen, int yScreen, bool twoPlayerMode) {
  if (!twoPlayerMode) {
    // 1P: top half = +1, bottom half = -1.
    change(yScreen < CENTER_Y ? +1 : -1, twoPlayerMode);
  } else {
    // 2P across: each player's own right hand = +1, left hand = -1.
    // P2 lives in a 180° container so their right hand is screen-left.
    const bool rightOfScreen = (xScreen >= CENTER_X);
    const bool isPlus = _isP2 ? !rightOfScreen : rightOfScreen;
    change(isPlus ? +1 : -1, twoPlayerMode);
  }
}

// ---- layout ---------------------------------------------------------------

void LifeCounter::centerFull() {
  _lastOy = 0;
  repositionLifeLabel(0);
  lv_obj_update_layout(_label);
  repositionDelta();
}

void LifeCounter::centerHalf(bool topSide) {
  // 2P across: P2 sits above the divider, P1 below.
  const int oy = topSide ? -Theme::Game::CounterOy2P : +Theme::Game::CounterOy2P;
  _lastOy = oy;
  repositionLifeLabel(oy);
  lv_obj_update_layout(_label);
  repositionDelta();
}

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
  if (!visible) hideDelta();
}

// ---- per-loop tick --------------------------------------------------------

void LifeCounter::updateDelta(uint32_t now) {
  // ---- 1. delta bundle ----
  //
  // Close the open bundle after BUNDLE_MS of inactivity. The bundle's
  // undo snapshot stays on the stack -- only the visible badge fades.
  if (_bundleOpen && !_undoPending && (now - _bundleLastAt) >= BUNDLE_MS) {
    _bundleOpen = false;
    hideDelta();
  }

  // ---- 2. rejected-input feedback ----
  // Three layered cues fire over LIFE_BUMP_MS so a hit on min/max is
  // unmistakable: horizontal head-shake (two left-right swings),
  // brief grey colour flash, and a subtle opacity dip.
  //
  // We do NOT use transform_scale here -- scaling the large custom
  // life font on ESP32-S3 + LVGL partial rendering has been observed
  // to lock up under rapid taps. lv_obj_set_style_translate_x is safe
  // because it doesn't re-rasterize the glyph.
  if (_bumpActive) {
    const uint32_t elapsed = now - _bumpStartAt;
    if (elapsed >= LIFE_BUMP_MS) {
      _bumpActive = false;
      lv_obj_set_style_text_opa(_label, LV_OPA_COVER, 0);
      lv_obj_set_style_translate_x(_label, 0, 0);
      // Restore the colour the normal cascade would pick now.
      refreshLabel();
    } else {
      // Horizontal head-shake: damped sine, ~2 full oscillations.
      // sin(2pi * 2 * t/dur) with linear decay to zero amplitude.
      const float t = (float)elapsed / (float)LIFE_BUMP_MS;
      const float decay = 1.0f - t;
      const float angle = t * 2.0f * 2.0f * PI;  // 2 oscillations
      const int dx = (int)lroundf(sinf(angle) * LIFE_BUMP_SHAKE_AMP * decay);

      lv_obj_set_style_translate_x(_label, dx, 0);

      // Subtle opacity dip -- peaks at mid-duration.
      const uint32_t half = LIFE_BUMP_MS / 2;
      const uint32_t safeHalf = half ? half : 1;
      const uint32_t dt = elapsed < half ? elapsed : (LIFE_BUMP_MS - elapsed);
      const uint8_t dip = 60;
      const uint8_t opa = LV_OPA_COVER - (uint8_t)((dip * dt) / safeHalf);
      lv_obj_set_style_text_opa(_label, opa, 0);

      // Force the counter colour to a deep grey for the whole bump,
      // so a max-hit reads as "input rejected / disabled" rather than
      // being mistaken for actual damage (which would be red).
      // Restored on bump-end via refreshLabel() above.
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

      // Finish only when the counter actually reached the target.
      // Do not force-jump to max based on elapsed time.
      if (_value == _resetTo) {
        _resetActive = false;
      }
    }

    return;
  }

  // ---- 4. low-HP pulse ----
  updatePulse(now);
}

// ---- private --------------------------------------------------------------

// Position the main life number.
//
//   1P:        centred on the screen (oy == 0).
//   2P across: horizontally centred, vertically shifted into the player's
//              half. The horizontal divider sits at y == CENTER_Y, the
//              counter centre at y == CENTER_Y +/- Y_OFFSET_2P.
void LifeCounter::repositionLifeLabel(int oy) {
  if (!_label) return;
  lv_obj_align(_label, LV_ALIGN_CENTER, 0, oy);
}

// Anchor the delta badge above the counter in screen coordinates.
// Both players use TOP_MID: for P1 (bottom half) this is physically above
// the counter; for P2 (top half, label rotated 180°) TOP_MID lands between
// the counter and P2's bezel, keeping the badge inside P2's half.
// The badge itself is also rotated 180° so P2 reads it correctly.
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
  // Badge colour follows "did this heal or damage us?", same flip as flash.
  const bool isHealing = _countUp ? (accDelta < 0) : (accDelta > 0);
  lv_obj_set_style_bg_color(_deltaLbl, isHealing ? COLOR_PLUS : COLOR_MINUS, 0);
  lv_obj_remove_flag(_deltaLbl, LV_OBJ_FLAG_HIDDEN);
  // Anchor must happen AFTER unhide so the
  // hidden-flag short-circuit in repositionDelta() doesn't skip the work.
  repositionDelta();
}

void LifeCounter::hideDelta() {
  if (_deltaLbl) lv_obj_add_flag(_deltaLbl, LV_OBJ_FLAG_HIDDEN);
}

void LifeCounter::startBump() {
  // Restart the short rejected-input feedback, but leave all transforms alone.
  _bumpActive = true;
  _bumpStartAt = Clock::now();
}

// Map distance-to-defeat onto a colour zone. Single source of truth
// for thresholds -- used by both main counter and sub-label.
lv_color_t LifeCounter::zoneColor(int distance) const {
  if (distance == 0) return COLOR_MINUS;
  if (distance <= LIFE_ZONE_RED_MAX) return COLOR_MINUS;
  if (distance <= LIFE_ZONE_YELLOW_MAX) return COLOR_BAT_YELLOW;
  return COLOR_FG;
}

// Ambient opacity throb while in the red zone (distance 1..RED_MAX).
// Triangle wave between LIFE_PULSE_OPA_MIN and LV_OPA_COVER over
// LIFE_PULSE_PERIOD_MS. Cancels itself cleanly when the zone changes.
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
  // Map t in [0..half] to opacity in [MIN..COVER].
  const uint8_t opa = LIFE_PULSE_OPA_MIN + (uint8_t)(((LV_OPA_COVER - LIFE_PULSE_OPA_MIN) * t) / half);
  lv_obj_set_style_text_opa(_label, opa, 0);
}

void LifeCounter::refreshLabel() {
  if (!_label) return;

  const int distance = distanceToDefeat();

  // ---- main counter ----
  lv_color_t mainColor = _undoPending ? COLOR_VALUE_GREY : zoneColor(distance);
  lv_obj_set_style_text_color(_label, mainColor, 0);
  lv_label_set_text_fmt(_label, "%d", _value);

  // Re-run the layout whenever the text changes. In 2P across the y
  // position is fixed (no width-dependent math), but we still want a
  // single layout call so the pivot adjustment below sees up-to-date
  // dimensions.
  lv_obj_update_layout(_label);
  repositionLifeLabel(_lastOy);
  // Badge position is fixated: set once at showDelta / layout change time,
  // not re-anchored on every value change. The TOP_MID / BOTTOM_MID anchor
  // stays centred regardless of digit-count width changes.
}

// ---- undo history ---------------------------------------------------------

void LifeCounter::pushUndo(int valueBefore) {
  _undoBefore[_undoHead] = valueBefore;
  _undoHead = (_undoHead + 1) % UNDO_HISTORY_DEPTH;
  if (_undoCount < UNDO_HISTORY_DEPTH) _undoCount++;
  // When full, the oldest entry is overwritten silently. The user keeps
  // their most recent UNDO_HISTORY_DEPTH bundles -- older taps drop off
  // the bottom of the stack rather than blocking the new push.
}

int LifeCounter::popUndo() {
  // Caller must check canUndo() first.
  _undoHead = (_undoHead + UNDO_HISTORY_DEPTH - 1) % UNDO_HISTORY_DEPTH;
  _undoCount--;
  return _undoBefore[_undoHead];
}

int LifeCounter::peekUndo() const {
  // Caller must check canUndo() first.
  const uint8_t idx = (_undoHead + UNDO_HISTORY_DEPTH - 1) % UNDO_HISTORY_DEPTH;
  return _undoBefore[idx];
}

void LifeCounter::clearUndoHistory() {
  _undoHead = 0;
  _undoCount = 0;
}
