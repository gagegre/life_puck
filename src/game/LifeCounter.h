// LifeCounter.h
//
// One player's life value and its on-screen rendering. Tracks the
// counter plus the secondary "damage/base" sub-label and a delta badge,
// and owns four overlapping animations:
//
//   1. Bundle / delta-badge fade (shows e.g. "+3" after rapid taps)
//   2. Bump / rejected-input wobble when a tap is clamped by 0 or base
//   3. Reset celebration (animate to target value instead of jumping)
//   4. Low-HP ambient opacity pulse when close to defeat
//
// Two display modes are unified via the "distance to defeat" abstraction
// so colour, pulse and defeat decisions are identical regardless of
// which way the number moves:
//
//   count-down : value = HP remaining; distance = value
//   count-up   : value = damage taken; distance = base - value

#pragma once

#include "Config.h"

#include <lvgl.h>

class FlashManager;

class LifeCounter {
public:
  // Vertical offset of the sub-label below the counter (and the delta
  // badge above it). Same in 1P and 2P — spacing reads identically.
  static constexpr int LABEL_DY = 52;
  static constexpr uint32_t BUNDLE_MS = 1500;

  // Fires when this counter just transitioned to distance == 0.
  // The signature is a plain function pointer (not std::function) to
  // keep allocation cost zero on a 320 KB-RAM target.
  using DefeatCb = void (*)(int /*playerIdx 0 or 1*/);
  void setDefeatCallback(DefeatCb cb) {
    _defeatCb = cb;
  }

  // Build LVGL labels and apply orientation. `flipped` is true for the
  // P2 counter in 2P mode (rendered upside-down for the opponent).
  void begin(lv_obj_t* parent, FlashManager* flash,
             bool isP2 = false, bool flipped = false);

  // ---- mutation ---------------------------------------------------------

  // Apply a signed delta. Clamps at [LIFE_MIN, _baseLife]. Fires the
  // flash arc and (if the player just reached distance 0) the defeat
  // callback. Out-of-bounds taps trigger the rejected-input bump.
  void change(int delta, bool twoPlayerMode);

  // Undo the entire current bundle in one step. Returns false if nothing.
  bool undo();
  bool canUndo() const {
    return _bundleOrigin >= 0;
  }

  // Two-step swipe-undo: first swipe puts the counter into the
  // "pending" visual state (dimmed + orange undo glyph); a second
  // confirming swipe calls undo(). Any other action clears.
  bool beginUndoPending();
  void clearUndoPending();

  void setValue(int v);

  void setBaseLife(int base);
  int getBaseLife() const {
    return _baseLife;
  }

  void setCountUp(bool up);
  bool isCountUp() const {
    return _countUp;
  }

  // Reset to the starting value for the current mode.
  //
  // The animation ALWAYS plays the FULL 0↔MAX sweep, regardless of the
  // current _value:
  //   count-down: animate 0 → baseLife    (fill up to ready)
  //   count-up:   animate baseLife → 0    (clear damage to ready)
  //
  // We never short-circuit when _value already equals the target — that
  // would make a shake-reset at full HP, or a mode switch with matching
  // values, silently do nothing. The dramatic count is the player's main
  // confirmation that the reset happened, so it must always be visible.
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

  // Tap: top half = +1, bottom half = -1 (inverted for the flipped P2 view).
  void tapped(int yScreen, bool twoPlayerMode);

  // ---- layout ------------------------------------------------------------

  // 1P layout: counter centred on the screen.
  void centerFull();
  // 2P layout: counter aligned to one side of the centre divider.
  void centerHalf(bool leftSide);

  void useFont(const lv_font_t* f);

  void setVisible(bool visible);

  // ---- per-loop tick -----------------------------------------------------
  //
  // Drives the four animations. Call once per loop iteration with the
  // current Clock::now() timestamp.
  void updateDelta(uint32_t now);

  lv_obj_t* lvObj() const {
    return _label;
  }

private:
  // px between sub-label inner edge and divider in 2P mode.
  static constexpr int DIVIDER_GAP = 20;

  // ---- value state ------------------------------------------------------
  int _value = STARTING_LIFE;
  int _baseLife = STARTING_LIFE;
  bool _countUp = false;

  // ---- bundle / undo / delta state --------------------------------------
  int _bundleOrigin = -1;
  int _accDelta = 0;
  bool _bundleOpen = false;
  uint32_t _bundleLastAt = 0;
  bool _undoPending = false;

  // ---- animation state --------------------------------------------------
  bool _bumpActive = false;
  uint32_t _bumpStartAt = 0;
  bool _resetActive = false;
  uint32_t _resetStartAt = 0;
  int _resetFrom = 0;
  int _resetTo = 0;
  bool _pulsing = false;

  // ---- LVGL handles -----------------------------------------------------
  lv_obj_t* _parent = nullptr;
  lv_obj_t* _label = nullptr;
  lv_obj_t* _baseLbl = nullptr;
  lv_obj_t* _deltaLbl = nullptr;
  FlashManager* _flash = nullptr;
  bool _isP2 = false;
  bool _flipped = false;
  // 0 in 1P; -1/+1 in 2P so refreshLabel() can re-run divider-edge
  // alignment whenever the digit width changes.
  int _lastOx = 0;
  DefeatCb _defeatCb = nullptr;

  // ---- helpers ----------------------------------------------------------
  void applyFlip(lv_obj_t* obj);
  void updatePivot(lv_obj_t* obj);
  void repositionMainLabel(int ox);
  void repositionSubLabels(int ox);
  void showDelta(int accDelta);
  void hideDelta();
  void startBump();
  lv_color_t zoneColor(int distance) const;
  void updatePulse(uint32_t now);
  void refreshLabel();
};
