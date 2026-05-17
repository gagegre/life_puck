// GameUi.h
//
// Owns the in-game widgets (two LifeCounters, the centre divider) and
// the 1P <-> 2P layout swap. Replaces the loose enter2PLayout /
// exit2PLayout free functions and the visibility coupling with Battery
// that used to live as reach-into globals.
//
// Also declares the GameState struct that holds the runtime mode flags
// (1P/2P, count-up, touch-lock, base-life values), and the
// fontForMode() helper that picks the right custom font for the
// current mode.

#pragma once

#include "Config.h"
#include "LifeCounter.h"

#include <lvgl.h>

class FlashManager;
class Battery;

// Runtime game-mode flags. Two-player toggle, count direction, etc.
struct GameState {
  bool countUp = false;
  bool twoPlayer = false;
  bool touchLocked = false;
  int baseLife1 = 30;
  int baseLife2 = 30;
};

// Picks the right custom font for the current mode.
//   1P -> larger font (life_font_96)
//   2P -> smaller font (life_font_72)
inline const lv_font_t* fontForMode(bool twoPlayerMode) {
  return twoPlayerMode ? &life_font_72 : &life_font_96;
}

class GameUi {
public:
  void begin(lv_obj_t* parent, FlashManager* flash, Battery* battery);

  LifeCounter& p1() {
    return _p1;
  }
  LifeCounter& p2() {
    return _p2;
  }

  void enterTwoPlayer();
  void exitTwoPlayer();

  // Hide the game widgets while the radial menu is open. Battery is
  // hidden too so the radial overlay stays uncluttered.
  void hideForMenu();

  // Restore the game widgets after the radial menu closes.
  void showAfterMenu(bool twoPlayerMode);

  // Hide/show the full game-counter layer for global modal states such
  // as BASE LOST. Unlike hideForMenu(), this hides base labels and
  // delta badges too, not just the large number label.
  void setCountersVisible(bool visible, bool twoPlayerMode);

  void resetBoth(bool countUpMode, bool twoPlayerMode);

  // Used after wake-from-deep-sleep to restore exact values.
  void restoreValues(int p1Life, int p2Life);

  void setBaseLife1(int v) {
    _p1.setBaseLife(v);
  }
  void setBaseLife2(int v) {
    _p2.setBaseLife(v);
  }

  void setCountUp(bool up) {
    _p1.setCountUp(up);
    _p2.setCountUp(up);
  }

  // Cancel any in-flight undo-pending state on both players.
  void clearAllUndoPending() {
    _p1.clearUndoPending();
    _p2.clearUndoPending();
  }

  // Tick the per-player delta badges so they fade on schedule.
  void updateDeltas(uint32_t now) {
    _p1.updateDelta(now);
    _p2.updateDelta(now);
  }

  // Undo the last change for the given player (0 = P1, 1 = P2).
  bool undoPlayer(int player) {
    if (player == 1) return _p2.undo();
    return _p1.undo();
  }

private:
  lv_obj_t* _divider = nullptr;
  LifeCounter _p1;
  LifeCounter _p2;
  Battery* _battery = nullptr;
  bool _twoPlayerMode = false;

  void applyFonts() {
    const lv_font_t* f = fontForMode(_twoPlayerMode);
    _p1.useFont(f);
    _p2.useFont(f);
  }
};
