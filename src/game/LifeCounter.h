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
  // Sub-label offsets and counter position are in Theme::Game
  // (SubLabelDy, SubLabelDy2P, CounterOy2P). The delta badge is anchored
  // directly to the counter's top-right corner via lv_obj_align_to.

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
  void begin(lv_obj_t* parent, FlashManager* flash, bool isP2 = false, bool flipped = false);

  // ---- mutation ---------------------------------------------------------

  // Apply a signed delta. Clamps at [LIFE_MIN, _baseLife]. Fires the
  // flash arc and (if the player just reached distance 0) the defeat
  // callback. Out-of-bounds taps trigger the rejected-input bump.
  void change(int delta, bool twoPlayerMode);

  // Undo the most recent bundle, restoring the value to its pre-bundle
  // state. Returns false if the undo history is empty.
  bool undo();
  bool canUndo() const {
    return _undoCount > 0;
  }

  // Two-step swipe-undo: first swipe puts the counter into the
  // "pending" visual state (dimmed + orange undo glyph); a second
  // confirming swipe calls undo(). Any other action clears.
  bool beginUndoPending();
  void clearUndoPending();

  // Direct value setter. Bypasses bundle/undo bookkeeping -- intended
  // for wake-from-deep-sleep restore. Clears the undo history because
  // the new value is not a gameplay action you'd want to walk back.
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
  // The animation ALWAYS plays the FULL 0..MAX sweep, regardless of the
  // current _value:
  //   count-down: animate 0 -> baseLife    (fill up to ready)
  //   count-up:   animate baseLife -> 0    (clear damage to ready)
  //
  // We never short-circuit when _value already equals the target -- that
  // would make a shake-reset at full HP, or a mode switch with matching
  // values, silently do nothing. The dramatic count is the player's main
  // confirmation that the reset happened, so it must always be visible.
  // Clears the undo history -- a fresh game is not "undoable".
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

  // Tap: routes the touch to a +/-1 change based on the current layout.
  // The axis is left/right in both 1P and 2P-across, so the rule
  // "each player's right hand = +1, left hand = -1" holds in both
  // modes. In 2P, P2 is rotated 180 deg, so P2's right hand maps to
  // screen-left -- the flip is handled internally.
  void tapped(int xScreen, int yScreen, bool twoPlayerMode);

  // ---- layout ------------------------------------------------------------

  // 1P layout: counter centred on the screen.
  void centerFull();
  // 2P across layout: counter aligned to its half of the screen, above
  // (P2) or below (P1) the horizontal centre divider.
  void centerHalf(bool topSide);

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
  // ---- value state ------------------------------------------------------
  int _value = STARTING_LIFE;
  int _baseLife = STARTING_LIFE;
  bool _countUp = false;

  // ---- undo history -----------------------------------------------------
  //
  // Ring buffer of pre-bundle values. `_undoHead` is the next-write
  // position; `_undoCount` is the current size (0..UNDO_HISTORY_DEPTH).
  // When full, the oldest entry is silently dropped.
  int _undoBefore[UNDO_HISTORY_DEPTH] = {};
  uint8_t _undoHead = 0;
  uint8_t _undoCount = 0;

  // ---- bundle / delta state ---------------------------------------------
  //
  // While a bundle is open and within BUNDLE_MS of the last tap, repeated
  // taps accumulate into the same delta badge and share a single undo
  // snapshot. After BUNDLE_MS without activity the badge fades but the
  // snapshot stays on the undo stack so the user can still walk it back.
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

  // ---- LVGL handles -----------------------------------------------------
  lv_obj_t* _parent = nullptr;
  lv_obj_t* _label = nullptr;
  lv_obj_t* _baseLbl = nullptr;
  lv_obj_t* _deltaLbl = nullptr;
  FlashManager* _flash = nullptr;
  bool _isP2 = false;
  bool _flipped = false;
  // 0 in 1P; +/-Y_OFFSET_2P in 2P-across. Stored so refreshLabel() can
  // re-run the layout when the digit width changes (e.g. transition
  // between 1- and 2-digit values during the reset animation).
  int _lastOy = 0;
  DefeatCb _defeatCb = nullptr;

  // ---- helpers ----------------------------------------------------------
  void applyFlip(lv_obj_t* obj);
  void updatePivot(lv_obj_t* obj);
  void repositionMainLabel(int oy);
  void repositionSubLabels(int oy);
  // Anchor the delta badge to the top-right corner of the counter, in
  // the player's reading frame. No-op while the badge is hidden.
  void repositionDelta();
  void showDelta(int accDelta);
  void hideDelta();
  void startBump();
  lv_color_t zoneColor(int distance) const;
  void updatePulse(uint32_t now);
  void refreshLabel();

  // ---- undo helpers -----------------------------------------------------
  void pushUndo(int valueBefore);
  int popUndo();
  int peekUndo() const;
  void clearUndoHistory();
};
