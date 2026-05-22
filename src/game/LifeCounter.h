// LifeCounter.h
//
// The 1P life value and its on-screen rendering. Tracks the counter plus the
// delta badge plus the "OF XY" base-life reveal label, and owns the small
// set of animations that the counter can play:
//
//   1. Bundle / delta-badge fade (shows e.g. "+3" after rapid taps)
//   2. Bump / rejected-input wobble when a tap is clamped by 0 or base
//   3. Reset celebration (animate to target value instead of jumping)
//   4. Low-HP ambient opacity pulse when close to defeat
//   5. OF XY base-life reveal slide (counter shifts up, base label slides in)
//
// Two display modes are unified via the "distance to defeat" abstraction
// so colour, pulse and defeat decisions are identical regardless of
// which way the number moves:
//
//   count-down : value = HP remaining; distance = value
//   count-up   : value = damage taken; distance = base - value
//
// Undo
//   Each finalized bundle is pushed onto a fixed-size ring of recent
//   bundle origins (UNDO_HISTORY_DEPTH deep). undo() pops the most
//   recent and restores the value to its pre-bundle state, so the user
//   can walk back several miscounted steps. reset() and setValue() both
//   clear the history -- those are not gameplay actions to "undo".

#pragma once

#include "Theme.h"

#include <lvgl.h>

class FlashManager;

class LifeCounter {
public:
  static constexpr uint32_t BUNDLE_MS = 1500;

  // Fires when this counter just transitioned to distance == 0.
  using DefeatCb = void (*)();
  void setDefeatCallback(DefeatCb cb) {
    _defeatCb = cb;
  }

  void begin(lv_obj_t* parent, FlashManager* flash);

  // ---- mutation ---------------------------------------------------------

  // Apply a signed delta. Clamps at [LIFE_MIN, _baseLife]. Fires the
  // flash arc and (if the player just reached distance 0) the defeat
  // callback. Out-of-bounds taps trigger the rejected-input bump.
  void change(int delta);

  // Undo the most recent bundle. Returns false if history is empty.
  bool undo();
  bool canUndo() const {
    return _undoCount > 0;
  }

  // Two-step swipe-undo helpers.
  bool beginUndoPending();
  void clearUndoPending();

  // Direct value setter (e.g. wake-from-deep-sleep restore). Clears undo
  // history because the new value isn't a gameplay action.
  void setValue(int v);

  void setBaseLife(int base);
  int getBaseLife() const {
    return _baseLife;
  }

  void setCountUp(bool up);
  bool isCountUp() const {
    return _countUp;
  }

  // Reset to the starting value for the current mode (full animated sweep).
  void reset(bool countUpMode);

  int getValue() const {
    return _value;
  }

  // Distance-to-defeat: unifies the two modes. 0 = defeated.
  int distanceToDefeat() const {
    return _countUp ? (_baseLife - _value) : _value;
  }
  bool isDefeated() const {
    return distanceToDefeat() == 0;
  }

  // Tap: 1P only -- top half = +1, bottom half = -1.
  void tapped(int yScreen);

  // ---- OF XY base-life reveal -------------------------------------------
  void showBaseReveal();
  void hideBaseReveal();
  bool isBaseRevealActive() const {
    return _baseRevealActive;
  }

  // ---- layout / visibility ----------------------------------------------
  void useFont(const lv_font_t* f);
  void setVisible(bool visible);

  // Per-loop tick. Drives all animations + the OF XY auto-hide timer.
  void update(uint32_t now);

  lv_obj_t* lvObj() const {
    return _label;
  }

private:
  // ---- value state ------------------------------------------------------
  int _value = STARTING_LIFE;
  int _baseLife = STARTING_LIFE;
  bool _countUp = false;

  // ---- undo history -----------------------------------------------------
  int _undoBefore[UNDO_HISTORY_DEPTH] = {};
  uint8_t _undoHead = 0;
  uint8_t _undoCount = 0;

  // ---- bundle / delta state ---------------------------------------------
  int _accDelta = 0;
  bool _bundleOpen = false;
  uint32_t _bundleLastAt = 0;
  bool _undoPending = false;

  // ---- animation state --------------------------------------------------
  bool _bumpActive = false;
  uint32_t _bumpStartAt = 0;
  bool _resetActive = false;
  uint32_t _resetStartAt = 0;
  uint32_t _resetLastStepAt = 0;
  int _resetFrom = 0;
  int _resetTo = 0;
  bool _pulsing = false;
  // Last digit-count we ran update_layout for. We only re-layout when
  // the width may have changed (digit count delta), not on every tap.
  int _lastDigitCount = 0;

  // ---- OF XY base reveal ------------------------------------------------
  bool _baseRevealActive = false;
  uint32_t _baseRevealAt = 0;

  // ---- LVGL handles -----------------------------------------------------
  lv_obj_t* _parent = nullptr;
  lv_obj_t* _label = nullptr;
  lv_obj_t* _deltaLbl = nullptr;
  lv_obj_t* _baseLbl = nullptr;
  FlashManager* _flash = nullptr;
  DefeatCb _defeatCb = nullptr;

  // ---- helpers ----------------------------------------------------------
  void repositionLifeLabel();
  void repositionDelta();
  void repositionBaseLabel();
  void refreshLabel();
  void showDelta(int accDelta);
  void hideDelta();
  void refreshBaseLabelText();
  void startBump();
  lv_color_t zoneColor(int distance) const;
  void updatePulse(uint32_t now);

  // ---- undo helpers -----------------------------------------------------
  void pushUndo(int valueBefore);
  int popUndo();
  int peekUndo() const;
  void clearUndoHistory();
};
