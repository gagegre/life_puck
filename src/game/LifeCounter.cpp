// LifeCounter.cpp -- see LifeCounter.h for behaviour overview.

#include "LifeCounter.h"
#include "FlashManager.h"
#include "Clock.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>

// ---- lifecycle ------------------------------------------------------------

void LifeCounter::begin(lv_obj_t* parent, FlashManager* flash, bool isP2, bool flipped) {
  _parent = parent;
  _flash = flash;
  _isP2 = isP2;
  _flipped = flipped;

  // ---- main life label ----
  _label = lv_label_create(parent);
  lv_obj_set_style_text_color(_label, COLOR_FG, 0);
  lv_obj_set_style_text_font(_label, &life_font_96, 0);
  applyFlip(_label);

  // ---- base / damage label ("8/30") ----
  _baseLbl = lv_label_create(parent);
  lv_obj_set_style_text_font(_baseLbl, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(_baseLbl, lv_color_hex(0x666666), 0);
  lv_label_set_text(_baseLbl, "");
  applyFlip(_baseLbl);

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
  applyFlip(_deltaLbl);

  refreshLabel();
  centerFull();
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
  //   position = the player's "+/-" rim zone (1P: right/left half;
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
    updatePivot(_deltaLbl);
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
  // Unified left/right tap axis across 1P and 2P-across so the mental
  // model is the same in both modes: each player's own right hand = +1,
  // left hand = -1. In 2P, P2 is rotated 180 deg, so P2's own right
  // hand reaches toward screen-LEFT -- the _flipped branch (set only
  // for P2) inverts the sign so the rule holds in P2's frame too.
  (void)yScreen;
  const bool rightOfScreen = (xScreen >= CENTER_X);
  const bool isPlus = _flipped ? !rightOfScreen : rightOfScreen;
  change(isPlus ? +1 : -1, twoPlayerMode);
}

// ---- layout ---------------------------------------------------------------

void LifeCounter::centerFull() {
  _lastOy = 0;
  repositionMainLabel(0);
  repositionSubLabels(0);
  repositionDelta();
}

void LifeCounter::centerHalf(bool topSide) {
  // 2P across: P2 sits above the divider, P1 below. Sub-labels follow
  // along on the player's "below the counter" side (handled by _flipped
  // inside repositionSubLabels).
  const int oy = topSide ? -Y_OFFSET_2P : +Y_OFFSET_2P;
  _lastOy = oy;
  repositionMainLabel(oy);
  repositionSubLabels(oy);
  repositionDelta();
}

void LifeCounter::useFont(const lv_font_t* f) {
  lv_obj_set_style_text_font(_label, f, 0);
  if (_flipped) {
    lv_obj_update_layout(_label);
    lv_obj_set_style_transform_pivot_x(_label, lv_obj_get_width(_label) / 2, 0);
    lv_obj_set_style_transform_pivot_y(_label, lv_obj_get_height(_label) / 2, 0);
  }
  // Counter's bounding box just changed; keep the badge stuck to its
  // (new) top-right corner. No-op while the badge is hidden.
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
  setHide(_baseLbl, !visible);
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

      // LVGL can leave a grey redraw artifact with a 180 degree rotated label
      // when translate_x is animated. P2 is already flipped, so keep the
      // rejected-input feedback as opacity/grey only.
      lv_obj_set_style_translate_x(_label, _flipped ? 0 : dx, 0);

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
      lv_obj_set_style_text_color(_label, lv_color_hex(0x555555), 0);
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

void LifeCounter::applyFlip(lv_obj_t* obj) {
  if (!_flipped || !obj) return;
  lv_obj_set_style_transform_rotation(obj, 1800, 0);
  lv_obj_update_layout(obj);
  lv_obj_set_style_transform_pivot_x(obj, lv_obj_get_width(obj) / 2, 0);
  lv_obj_set_style_transform_pivot_y(obj, lv_obj_get_height(obj) / 2, 0);
}

void LifeCounter::updatePivot(lv_obj_t* obj) {
  if (!_flipped || !obj) return;
  lv_obj_update_layout(obj);
  lv_obj_set_style_transform_pivot_x(obj, lv_obj_get_width(obj) / 2, 0);
  lv_obj_set_style_transform_pivot_y(obj, lv_obj_get_height(obj) / 2, 0);
}

// Position the main life number.
//
//   1P:        centred on the screen (oy == 0).
//   2P across: horizontally centred, vertically shifted into the player's
//              half. The horizontal divider sits at y == CENTER_Y, the
//              counter centre at y == CENTER_Y +/- Y_OFFSET_2P.
void LifeCounter::repositionMainLabel(int oy) {
  if (!_label) return;
  lv_obj_align(_label, LV_ALIGN_CENTER, 0, oy);
}

// Position the secondary base/damage label relative to the main counter.
//
// The base label sits BELOW the counter in the player's own frame.
// For P1 (unflipped) that's larger screen-y, for P2 (flipped) that's
// smaller screen-y. Same convention in 1P and 2P across: the base
// label is always the "footnote" under the big number from each
// player's reading direction.
//
// 2P across uses a slightly tighter LABEL_DY_2P so the base sub-label
// stays inside the round bezel at y ~= +/-108.
//
// The delta badge is NOT positioned here -- it's anchored directly to
// the counter via repositionDelta() so it tracks the counter's actual
// bounding box (which changes with digit count).
void LifeCounter::repositionSubLabels(int oy) {
  const bool twoPAcross = (oy != 0);
  const int dy = twoPAcross ? LABEL_DY_2P : LABEL_DY;
  const int baseDy = _flipped ? -dy : dy;

  if (_baseLbl) lv_obj_align(_baseLbl, LV_ALIGN_CENTER, 0, oy + baseDy);
}

// Anchor the delta badge in the PLAYER's reading frame.
//
//   1P              : top-centred above the counter (over the digit).
//   2P P1 (bottom)  : screen top-right corner of the counter.
//   2P P2 (top)     : screen bottom-left corner -- which P2 perceives
//                     as top-right after the 180 deg label rotation.
//
// 2P uses a corner anchor (rather than top-mid) because the badge would
// otherwise sit on or near the divider where the other player's badge
// might also land; offsetting it to the player's outer corner keeps the
// two players' deltas visually separated.
//
// lv_obj_align_to is computed on the unrotated bounding box, so the
// rotation transform on _deltaLbl doesn't change the math; it only
// affects how the glyphs render. The small offsets push the badge
// slightly past the edge so it reads as "hanging off" the counter.
//
// Called whenever the counter's geometry might have changed (digit
// width on refreshLabel, font change, layout shift between 1P/2P) AND
// whenever the badge is shown.
void LifeCounter::repositionDelta() {
  if (!_deltaLbl || !_label) return;
  if (lv_obj_has_flag(_deltaLbl, LV_OBJ_FLAG_HIDDEN)) return;

  // Both layouts must be current for align_to to compute correctly.
  lv_obj_update_layout(_label);
  lv_obj_update_layout(_deltaLbl);

  // _lastOy == 0 is the 1P layout (centerFull); any non-zero offset
  // means 2P-across (centerHalf).
  const bool twoPAcross = (_lastOy != 0);
  if (!twoPAcross) {
    lv_obj_align_to(_deltaLbl, _label, LV_ALIGN_OUT_TOP_MID, 0, -2);
    return;
  }

  const lv_align_t align = _flipped ? LV_ALIGN_OUT_LEFT_BOTTOM : LV_ALIGN_OUT_RIGHT_TOP;
  const int xOfs = _flipped ? -2 : 2;
  const int yOfs = _flipped ? 2 : -2;
  lv_obj_align_to(_deltaLbl, _label, align, xOfs, yOfs);
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
  updatePivot(_deltaLbl);
  lv_obj_remove_flag(_deltaLbl, LV_OBJ_FLAG_HIDDEN);
  // Anchor to counter's top-right corner. Must happen AFTER unhide so the
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
  const bool defeated = (distance == 0);

  // ---- main counter ----
  lv_color_t mainColor = _undoPending ? COLOR_VALUE_GREY : zoneColor(distance);
  lv_obj_set_style_text_color(_label, mainColor, 0);
  lv_label_set_text_fmt(_label, "%d", _value);

  // Re-run the layout whenever the text changes. In 2P across the y
  // position is fixed (no width-dependent math), but we still want a
  // single layout call so the pivot adjustment below sees up-to-date
  // dimensions.
  lv_obj_update_layout(_label);
  repositionMainLabel(_lastOy);
  if (_flipped) {
    lv_obj_set_style_transform_pivot_x(_label, lv_obj_get_width(_label) / 2, 0);
    lv_obj_set_style_transform_pivot_y(_label, lv_obj_get_height(_label) / 2, 0);
  }

  // ---- damage / base sub-label ----
  //
  // The sub-label always reads as "damage_taken / base", regardless of
  // mode. In count-down, damage = base - value. In count-up, damage
  // IS the value (the displayed counter itself is damage).
  //
  // Colour mirrors the main counter so the eye scans the two as one
  // unit: white when healthy, yellow / red as defeat approaches.
  if (!_baseLbl) return;

  lv_obj_set_style_text_font(_baseLbl, LV_FONT_DEFAULT, 0);

  const int damage = _countUp ? _value : (_baseLife - _value);
  char buf[16];
  if (damage == 0) {
    snprintf(buf, sizeof(buf), "/%d", _baseLife);
    lv_obj_set_style_text_color(_baseLbl, lv_color_hex(0x444444), 0);
  } else {
    snprintf(buf, sizeof(buf), "%d/%d", damage, _baseLife);
    // Dim grey for the normal zone, escalate with the main counter.
    lv_color_t subColor = defeated                             ? COLOR_MINUS
                          : (distance <= LIFE_ZONE_RED_MAX)    ? COLOR_MINUS
                          : (distance <= LIFE_ZONE_YELLOW_MAX) ? COLOR_BAT_YELLOW
                                                               : COLOR_VALUE_GREY;
    lv_obj_set_style_text_color(_baseLbl, subColor, 0);
  }
  lv_label_set_text(_baseLbl, buf);
  updatePivot(_baseLbl);
  // Sub-labels are horizontally centred under the counter in both modes
  // now, so the position only depends on _lastOy. The re-align is still
  // useful when the font/text changes the bounding box.
  repositionSubLabels(_lastOy);

  // The counter's width changes with digit count (e.g. "9" -> "10" during
  // the reset animation). Re-anchor the delta to the new top-right corner.
  // No-op while the badge is hidden.
  repositionDelta();
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
