// LifeCounter.cpp — see LifeCounter.h for behaviour overview.

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

  // Bundle logic: if a bundle is open and the last change was recent,
  // keep accumulating within the same undo/delta window.
  const bool inWindow = _bundleOpen && ((now - _bundleLastAt) < BUNDLE_MS);
  if (!inWindow) {
    _bundleOrigin = prev;
    _accDelta = 0;
    _bundleOpen = true;
  }

  _value = next;
  _accDelta += (next - prev);
  _bundleLastAt = now;

  refreshLabel();
  showDelta(_accDelta);

  // Flash arc:
  //   position = tap location (top vs bottom), always follows delta sign
  //   colour   = mode-dependent meaning: heal = green, damage = red
  // The two are decoupled so the visual matches both the finger
  // position and the in-game consequence simultaneously.
  const bool tappedTop = (delta > 0);
  const bool isHealing = _countUp ? (delta < 0) : (delta > 0);
  if (_flash) _flash->trigger(tappedTop, isHealing, _isP2, twoPlayerMode);

  // Defeat detection: distance == 0 means defeat in either mode.
  if (distanceToDefeat() == 0 && _defeatCb) _defeatCb(_isP2 ? 1 : 0);
}

bool LifeCounter::undo() {
  if (_bundleOrigin < 0) return false;
  _value = _bundleOrigin;
  _bundleOrigin = -1;
  _bundleOpen = false;
  _undoPending = false;
  _accDelta = 0;
  refreshLabel();
  hideDelta();
  return true;
}

bool LifeCounter::beginUndoPending() {
  if (!canUndo()) return false;
  _undoPending = true;
  if (_deltaLbl && _bundleOrigin >= 0) {
    const int restore = _bundleOrigin - _value;
    char buf[8];
    if (restore >= 0)
      snprintf(buf, sizeof(buf), "+%d", restore);
    else
      snprintf(buf, sizeof(buf), "%d", restore);
    lv_label_set_text(_deltaLbl, buf);
    lv_obj_set_style_bg_color(_deltaLbl, COLOR_MENU_ORANGE, 0);
    lv_obj_remove_flag(_deltaLbl, LV_OBJ_FLAG_HIDDEN);
    updatePivot(_deltaLbl);
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
  _bundleOrigin = -1;
  _bundleOpen = false;
  _accDelta = 0;
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
  _bundleOrigin = -1;
  _bundleOpen = false;
  _accDelta = 0;
  _undoPending = false;
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

void LifeCounter::tapped(int yScreen, bool twoPlayerMode) {
  const bool topOfScreen = (yScreen < CENTER_Y);
  const bool isPlus = _flipped ? !topOfScreen : topOfScreen;
  change(isPlus ? +1 : -1, twoPlayerMode);
}

// ---- layout ---------------------------------------------------------------

void LifeCounter::centerFull() {
  _lastOx = 0;
  repositionMainLabel(0);
  repositionSubLabels(0);
}

void LifeCounter::centerHalf(bool leftSide) {
  // In 2P, use only the side/sign as the layout anchor. The actual
  // x offset depends on the current text width so the inner edge can
  // stay a fixed distance from the divider.
  const int ox = leftSide ? -1 : +1;
  _lastOx = ox;
  repositionMainLabel(ox);
  repositionSubLabels(ox);
}

void LifeCounter::useFont(const lv_font_t* f) {
  lv_obj_set_style_text_font(_label, f, 0);
  if (_flipped) {
    lv_obj_update_layout(_label);
    lv_obj_set_style_transform_pivot_x(_label, lv_obj_get_width(_label) / 2, 0);
    lv_obj_set_style_transform_pivot_y(_label, lv_obj_get_height(_label) / 2, 0);
  }
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
  if (_bundleOpen && !_undoPending && (now - _bundleLastAt) >= BUNDLE_MS) {
    _bundleOpen = false;
    hideDelta();
  }

  // ---- 2. rejected-input feedback ----
  // Three layered cues fire over LIFE_BUMP_MS so a hit on min/max is
  // unmistakable: horizontal head-shake (two left-right swings),
  // brief grey colour flash, and a subtle opacity dip.
  //
  // We do NOT use transform_scale here — scaling the large custom
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
      // sin(2π · 2 · t/dur) with linear decay to zero amplitude.
      const float t = (float)elapsed / (float)LIFE_BUMP_MS;
      const float decay = 1.0f - t;
      const float angle = t * 2.0f * 2.0f * PI;  // 2 oscillations
      const int dx = (int)lroundf(sinf(angle) * LIFE_BUMP_SHAKE_AMP * decay);

      // LVGL can leave a grey redraw artifact with a 180° rotated label
      // when translate_x is animated. P2 is already flipped, so keep the
      // rejected-input feedback as opacity/grey only.
      lv_obj_set_style_translate_x(_label, _flipped ? 0 : dx, 0);

      // Subtle opacity dip — peaks at mid-duration.
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
//   1P: centred on the screen.
//   2P: inner edge sits DIVIDER_GAP px from the divider, matching the
//       base/damage label placement.
void LifeCounter::repositionMainLabel(int ox) {
  if (!_label) return;

  if (ox == 0) {
    lv_obj_center(_label);
    return;
  }

  lv_obj_update_layout(_label);
  const int labelW = lv_obj_get_width(_label);
  const int sign = (ox < 0) ? -1 : +1;
  const int bx = sign * (DIVIDER_GAP + labelW / 2);
  lv_obj_align(_label, LV_ALIGN_CENTER, bx, 0);
}

// Position the secondary labels relative to the main counter.
//
//   Base label : sits BELOW the counter.
//                 - 1P (ox == 0): centred under the counter.
//                 - 2P (ox != 0): its INNER edge (toward the divider)
//                                 sits near the divider, not centred
//                                 within the half. With a 2-digit
//                                 counter "10" on the left half, the
//                                 sub-label "5/30" hugs the divider on
//                                 the right of P1's half.
//   Delta badge: sits ABOVE the counter, always centred at ox.
//
// For the flipped (P2) label, on-screen y direction is inverted from
// P2's perspective, so the y components flip sign. The 2P x math is
// symmetric in |ox|, so the same sign(ox) logic handles both players.
void LifeCounter::repositionSubLabels(int ox) {
  const int baseDy = _flipped ? -LABEL_DY : LABEL_DY;
  const int deltaDy = _flipped ? LABEL_DY : -LABEL_DY;

  if (_baseLbl) {
    int bx = ox;  // 1P default: same centre as the counter
    if (ox != 0) {
      // 2P: inner edge of sub-label sits DIVIDER_GAP px from screen
      // centre. ox is negative for P1 (left half), positive for P2.
      lv_obj_update_layout(_baseLbl);
      const int subW = lv_obj_get_width(_baseLbl);
      const int sign = (ox < 0) ? -1 : +1;
      bx = sign * (DIVIDER_GAP + subW / 2);
    }
    lv_obj_align(_baseLbl, LV_ALIGN_CENTER, bx, baseDy);
    if (_deltaLbl) {
      lv_obj_align(_deltaLbl, LV_ALIGN_CENTER, bx, deltaDy);
    }
  }
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
// for thresholds — used by both main counter and sub-label.
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

  // Whenever the text changes the label's width may change. In 2P mode
  // the main life label is aligned by its INNER edge against the divider,
  // so re-run the x layout whenever the number changes.
  lv_obj_update_layout(_label);
  repositionMainLabel(_lastOx);
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

  if (_undoPending && _bundleOrigin >= 0) {
    lv_obj_set_style_text_font(_baseLbl, &font_awesome_icons, 0);
    lv_obj_set_style_text_opa(_baseLbl, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(_baseLbl, COLOR_MENU_ORANGE, 0);
    lv_label_set_text(_baseLbl, FA_ICON_UNDO);
    updatePivot(_baseLbl);
    repositionSubLabels(_lastOx);
    return;
  }

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
  // In 2P mode the sub-label position depends on its own width
  // (inner edge sits near the divider), so re-align whenever the
  // text changes. No-op in 1P beyond a redundant align call.
  repositionSubLabels(_lastOx);
}
