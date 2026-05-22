// GameUi.h
//
// Owns the life counter widget. Thin wrapper that constructs the LifeCounter
// once, forwards mode flag changes, and coordinates the few global show/hide
// transitions (radial-menu open/close, BASE LOST modal).
//
// Also declares the GameState struct that holds the runtime mode flags
// (count direction, touch-lock, base-life value).

#pragma once

#include "Config.h"
#include "LifeCounter.h"

#include <lvgl.h>

class FlashManager;
class Battery;

// Runtime game-mode flags.
struct GameState {
  bool countUp = false;
  bool touchLocked = false;
  int baseLife = 30;
};

class GameUi {
public:
  void begin(lv_obj_t* parent, FlashManager* flash, Battery* battery);

  LifeCounter& p() {
    return _p;
  }

  // Hide the game widgets while the radial menu is open. Battery is
  // hidden too so the radial overlay stays uncluttered.
  void hideForMenu();

  // Restore the game widgets after the radial menu closes.
  void showAfterMenu();

  // Hide/show the counter for global modal states such as BASE LOST.
  // Unlike hideForMenu(), this hides the delta badge and OF XY label too.
  void setCounterVisible(bool visible);

  // Reset the counter to the starting value for the current mode.
  void reset(bool countUpMode);

  // Used after wake-from-deep-sleep to restore the exact value.
  void restoreValue(int life);

  void setBaseLife(int v) {
    _p.setBaseLife(v);
  }

  void setCountUp(bool up) {
    _p.setCountUp(up);
  }

  // Cancel any in-flight undo-pending state.
  void clearUndoPending() {
    _p.clearUndoPending();
  }

  // Tick the delta badge so it fades on schedule, and time-out the OF XY
  // base-life reveal if it has been visible long enough.
  void update(uint32_t now) {
    _p.update(now);
  }

  // Show / hide the "OF XY" base-life label beneath the counter.
  void showBaseReveal() {
    _p.showBaseReveal();
  }
  void hideBaseReveal() {
    _p.hideBaseReveal();
  }
  bool isBaseRevealActive() const {
    return _p.isBaseRevealActive();
  }

private:
  LifeCounter _p;
  Battery* _battery = nullptr;
};
