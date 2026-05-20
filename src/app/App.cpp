// App.cpp
//
// Owns the singleton instances and the app-level functions that the
// .ino calls from setup()/loop(). Splitting this out keeps the .ino
// thin and lets PowerManager.cpp/etc. share the same instances via
// App.h includes.

#include "App.h"
#include "Hardware.h"
#include "PowerManager.h"
#include "Clock.h"

#include <esp_attr.h>
#include <Arduino.h>

// ---- Persistent (RTC) state ----
RTC_DATA_ATTR PersistentState rtcState;

// ---- Singleton instances ----
IMU imu;
NvmSettings nvm;
Battery battery;
Backlight backlight;
FlashManager flashMgr;
GameUi gameUi;
ModeToast modeToast;
RadialMenu radialMenu;
ResetPendingOverlay resetPendingOverlay;
ResetPending resetPending;
DefeatOverlay defeatOverlay;
UndoPending undoPending;
UndoPendingOverlay undoPendingOverlay;
TouchRouter touchRouter;
GameState game;

// ==============================================================
// UI construction
// ==============================================================

void createGameUI() {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

  // Battery arc ring first so flash arcs and life labels render on top.
  // The detail overlay, also created here, foregrounds itself on demand.
  battery.begin(scr);

  // Flash feedback arcs and life labels (both above the battery arc).
  flashMgr.begin(scr);
  gameUi.begin(scr, &flashMgr, &battery);
  modeToast.begin(scr);
  resetPendingOverlay.begin(scr);
  undoPendingOverlay.begin(scr);
  defeatOverlay.begin(scr, scr);

  // Wire the defeat callback so a counter hitting distance=0 fires the
  // full-screen overlay. The static lambda is needed because LifeCounter
  // takes a plain function pointer (kept small to avoid std::function on
  // a 320 KB-RAM target). The single global defeatOverlay handles both
  // players; player index is forwarded so the overlay can position later
  // (currently full-screen for either side).
  auto defeatCb = +[](int playerIdx) {
    gameUi.clearAllUndoPending();
    undoPending.cancel();
    undoPendingOverlay.hide();
    resetPending.cancel();
    resetPendingOverlay.hide();
    gameUi.setCountersVisible(false, game.twoPlayer);
    defeatOverlay.show(playerIdx, game.twoPlayer);
  };
  gameUi.p1().setDefeatCallback(defeatCb);
  gameUi.p2().setDefeatCallback(defeatCb);

  lv_obj_move_foreground(gameUi.p1().lvObj());
  lv_obj_move_foreground(gameUi.p2().lvObj());

  gameUi.setBaseLife1(game.baseLife1);
  gameUi.setBaseLife2(game.baseLife2);
  gameUi.setCountUp(game.countUp);
  if (game.twoPlayer)
    gameUi.enterTwoPlayer();
  else
    gameUi.exitTwoPlayer();
}

void createRadialMenuOverlay() {
  // Build this only after the first life-counter frame was flushed.
  // Otherwise the panel can briefly show the old pre-sleep radial/choice
  // image while waking from deep sleep. The overlay is hidden immediately
  // after construction and moved to the foreground only when show() runs.
  radialMenu.begin(lv_screen_active(), &game, &gameUi, &backlight, &battery);
}

// ==============================================================
// executeMenuAction
//
// Dispatcher for both top-level fire-and-stay-open actions and the
// hidden option-actions emitted by sub-views.
// ==============================================================

void executeMenuAction(MenuAction action) {
  switch (action) {
    case MenuAction::SLEEP:
      // Top-level sleep is only a request to show the confirm dial.
      // Actual deep sleep is triggered exclusively by SLEEP_OFF.
      break;

    case MenuAction::SLEEP_OFF:
      PowerManager::deepSleepManual();
      break;

    case MenuAction::PLAYER_TOGGLE:
      game.twoPlayer = !game.twoPlayer;
      gameUi.resetBoth(game.countUp);
      if (game.twoPlayer)
        gameUi.enterTwoPlayer();
      else
        gameUi.exitTwoPlayer();
      radialMenu.onGameStateChanged();
      modeToast.show(iconForAction(MenuAction::PLAYER_TOGGLE, game),
                     game.twoPlayer ? UiText::VERSUS : UiText::SINGLE,
                     COLOR_MENU_BLUE);
      break;

    case MenuAction::SET_1P:
      if (game.twoPlayer) {
        game.twoPlayer = false;
        gameUi.resetBoth(game.countUp);
        gameUi.exitTwoPlayer();
      }
      radialMenu.onGameStateChanged();
      modeToast.show(FA_ICON_SINGLE, UiText::SINGLE, COLOR_MENU_BLUE);
      break;

    case MenuAction::SET_2P:
      if (!game.twoPlayer) {
        game.twoPlayer = true;
        gameUi.resetBoth(game.countUp);
        gameUi.enterTwoPlayer();
      }
      radialMenu.onGameStateChanged();
      modeToast.show(FA_ICON_VERSUS, UiText::VERSUS, COLOR_MENU_BLUE);
      break;

    case MenuAction::COUNT_DIRECTION:
      game.countUp = !game.countUp;
      gameUi.setCountUp(game.countUp);
      gameUi.resetBoth(game.countUp);
      radialMenu.onGameStateChanged();
      modeToast.show(iconForAction(MenuAction::COUNT_DIRECTION, game),
                     game.countUp ? UiText::COUNT_UP : UiText::COUNT_DOWN,
                     COLOR_MENU_ORANGE);
      break;

    case MenuAction::COUNT_DOWN:
      if (game.countUp) {
        game.countUp = false;
        gameUi.setCountUp(false);
        gameUi.resetBoth(game.countUp);
      }
      radialMenu.onGameStateChanged();
      modeToast.show(FA_ICON_COUNT_DOWN, UiText::COUNT_DOWN, COLOR_MENU_ORANGE);
      break;

    case MenuAction::COUNT_UP:
      if (!game.countUp) {
        game.countUp = true;
        gameUi.setCountUp(true);
        gameUi.resetBoth(game.countUp);
      }
      radialMenu.onGameStateChanged();
      modeToast.show(FA_ICON_COUNT_UP, UiText::COUNT_UP, COLOR_MENU_ORANGE);
      break;

    case MenuAction::BASE_SELECTOR_COMMIT: {
      // BaseSelectorView has already written the chosen values into
      // game.baseLife1 / game.baseLife2; persist and apply them now.
      gameUi.setBaseLife1(game.baseLife1);
      gameUi.setBaseLife2(game.baseLife2);

      // Only after explicit confirm: reset both players to the new full base life.
      gameUi.resetBoth(game.countUp);

      nvm.setBaseLife1(game.baseLife1);
      nvm.setBaseLife2(game.baseLife2);
      radialMenu.onGameStateChanged();
      char buf[24];
      game.twoPlayer ? snprintf(buf, sizeof(buf), UiText::BASE_LIFE_FMT_2P, game.baseLife1, game.baseLife2)
                     : snprintf(buf, sizeof(buf), UiText::BASE_LIFE_FMT_1P, game.baseLife1);
      modeToast.show(FA_ICON_BASE_LIFE, buf, COLOR_MENU_ORANGE);
      break;
    }

    case MenuAction::BATTERY_CYCLE: {
      const uint8_t next = (static_cast<uint8_t>(battery.mode()) + 1) % 3;
      battery.setMode((BatteryMode)next);
      if (battery.mode() != BatteryMode::HIDE) battery.forceRefresh();
      nvm.setBatteryMode(battery.mode());
      radialMenu.onGameStateChanged();
      const char* modeText = (battery.mode() == BatteryMode::HIDE)   ? UiText::BATTERY_HIDE
                             : (battery.mode() == BatteryMode::AUTO) ? UiText::BATTERY_AUTO
                                                                     : UiText::BATTERY_SHOW;
      char toastBuf[24];
      snprintf(toastBuf, sizeof(toastBuf), UiText::BATTERY_FMT, modeText);
      modeToast.show(FA_ICON_BATTERY_THREE_QUARTERS, toastBuf, COLOR_MENU_PINK);
      break;
    }

    case MenuAction::BRIGHTNESS_CYCLE: {
      const int pct = backlight.asPercent();
      const int nextPct = (pct < 25) ? 25 : (pct < 50) ? 50 : (pct < 75) ? 75 : (pct < 100) ? 100 : 25;
      backlight.setPercent(nextPct);
      nvm.setBrightness(backlight.level());
      radialMenu.onGameStateChanged();
      char toastBuf[24];
      snprintf(toastBuf, sizeof(toastBuf), UiText::BRIGHTNESS_FMT, nextPct);
      modeToast.show(FA_ICON_BRIGHTNESS, toastBuf, COLOR_MENU_YELLOW);
      break;
    }

    // BATTERY / BRIGHTNESS open sub-views; the actual change happens
    // there. After the sub-view commits, persist the new value here.
    case MenuAction::BATTERY: {
      nvm.setBatteryMode(battery.mode());
      nvm.setBatteryShowPct(battery.isShowingPercent());
      const char* modeText = battery.isShowingPercent()              ? UiText::BATTERY_PERCENT_ON
                             : (battery.mode() == BatteryMode::HIDE) ? UiText::BATTERY_HIDE
                             : (battery.mode() == BatteryMode::AUTO) ? UiText::BATTERY_AUTO
                                                                     : UiText::BATTERY_SHOW;
      char toastBuf[24];
      snprintf(toastBuf, sizeof(toastBuf), UiText::BATTERY_FMT, modeText);
      modeToast.show(FA_ICON_BATTERY_THREE_QUARTERS, toastBuf, COLOR_MENU_PINK);
      break;
    }

    case MenuAction::BRIGHTNESS: {
      nvm.setBrightness(backlight.level());
      char toastBuf[24];
      snprintf(toastBuf, sizeof(toastBuf), UiText::BRIGHTNESS_FMT, backlight.asPercent());
      modeToast.show(FA_ICON_BRIGHTNESS, toastBuf, COLOR_MENU_YELLOW);
      break;
    }

    case MenuAction::NONE:
    default:
      break;
  }
}

// ==============================================================
// drainPendingMenuAction
//
// Pulls one menu action off the radial menu's queue. If the action is
// a sub-view commit (which should close the menu), close it first and
// arm a finger-swallow so the confirming touch doesn't leak through to
// the game UI. Then dispatch and persist NVS prefs.
// ==============================================================

void drainPendingMenuAction() {
  const MenuAction a = radialMenu.takePendingAction();
  if (a == MenuAction::NONE) return;

  // Sub-view commits close the menu; top-level cycles stay open.
  const bool closeActions[] = {a == MenuAction::SLEEP,
                               a == MenuAction::SLEEP_OFF,
                               a == MenuAction::SET_1P,
                               a == MenuAction::SET_2P,
                               a == MenuAction::COUNT_DOWN,
                               a == MenuAction::COUNT_UP,
                               a == MenuAction::BASE_SELECTOR_COMMIT};
  bool didClose = false;
  for (bool c : closeActions) {
    if (c) {
      radialMenu.close();
      didClose = true;
      break;
    }
  }
  if (didClose) {
    // The user's finger that just tapped the centre to confirm is still
    // physically pressed. Without this, the CST816S can deliver a
    // follow-up SINGLE_TAP for that same press and bump the life counter
    // unexpectedly.
    touchRouter.swallowUntilLift();
    touchRouter.swallowNextGesture();
  }
  executeMenuAction(a);
  // Save brightness/battery prefs whenever a sub-view committed them.
  // (Cheap because Preferences buffers writes.)
  nvm.setBrightness(backlight.level());
  nvm.setBatteryMode(battery.mode());
  nvm.setBatteryShowPct(battery.isShowingPercent());
}

void restartFromDefeatOverlay() {
  defeatOverlay.cancel();
  gameUi.setCountersVisible(true, game.twoPlayer);
  gameUi.resetBoth(game.countUp);
  undoPending.cancel();
  undoPendingOverlay.hide();
  gameUi.clearAllUndoPending();
  resetPending.cancel();
  resetPendingOverlay.hide();
  modeToast.show(FA_ICON_RESET, UiText::RESET, COLOR_MINUS);
  touchRouter.swallowUntilLift();
  backlight.recordActivity();
}

// ==============================================================
// handleTouch
//
// Touch input arrives two ways:
//   1. touch.data via the CST816S library - event-like, fires once per
//      gesture (tap, swipe up/down/left/right).
//   2. Hardware::readTouchFingerDownRaw() - direct I2C poll of
//      register 0x02, returns 1/0/-1 for finger-down state. Used
//      between gesture events to track holds and confirm finger-lift.
//
// Routing priority each tick:
//   1. Centre-hold soft timer elapsed   -> open radial menu
//   2. Reset confirmation pending       -> any gesture cancels it
//   3. No fresh gesture event           -> radial menu tick (if open),
//                                          finger-lift detection
//   4. Backlight off / first wake touch -> swallow, do not act
//   5. Radial menu open                 -> radialMenu.handleTouch
//   6. Centre dead-zone                 -> track hold; allow real
//                                          gestures, swallow idle
//   7. Touch-locked                     -> centre-hold only
//   8. Cooldown / per-session latch     -> drop redundant samples
//   9. Undo pending                     -> centre hold confirms; release is swallowed
//  10. Otherwise                        -> tap/swipe routes to LifeCounter
// ==============================================================

void handleTouch() {
  const uint32_t now = Clock::now();

  // Open the menu when the soft hold timer elapses. Armed confirmation
  // states own the centre hold, so they must not accidentally open RadialMenu.
  if (!radialMenu.isOpen() && !resetPending.active() && !undoPending.active() &&
      !defeatOverlay.isActive() && touchRouter.holdComplete(now)) {
    radialMenu.show();
    touchRouter.markHoldOpenedMenu();
    touchRouter.recordAction();
    backlight.recordActivity();
  }

  const bool hasTouched = Hardware::touch.available();
  const int gesture = hasTouched ? Hardware::touch.data.gestureID : 0;

  // While reset-pending, block normal game input.
  // Any deliberate gesture (tap or swipe) dismisses the confirmation.
  if (resetPending.active()) {
    touchRouter.resetHold();
    backlight.recordActivity();
    if (hasTouched && gesture != 0) {
      resetPending.cancel();
      resetPendingOverlay.hide();
    }
    return;
  }

  // While undo-pending, block normal game input. The swipe only arms undo;
  // confirmation starts after that swipe finger has lifted and the next
  // deliberate touch begins in the centre.
  if (undoPending.active()) {
    touchRouter.resetHold();
    backlight.recordActivity();

    // The undo-arming swipe calls swallowUntilLift(). Do not treat the tail of
    // that same swipe as a cancel gesture; just wait until the finger is really
    // up so the user can perform the centre hold cleanly.
    if (touchRouter.isSwallowing()) {
      touchRouter.onNoSample();
      return;
    }

    if (hasTouched) {
      const bool isCenter = abs(Hardware::touch.data.x - CENTER_X) <= CENTER_TAP_HALF &&
                            abs(Hardware::touch.data.y - CENTER_Y) <= CENTER_TAP_HALF;
      if (isCenter) {
        if (!undoPending.fingerDown()) undoPending.beginHold();
      } else if (gesture != 0) {
        undoPending.releaseHold();
        undoPendingOverlay.setProgress(0.0f);
        gameUi.clearAllUndoPending();
        undoPending.cancel();
        undoPendingOverlay.hide();
        touchRouter.swallowUntilLift();
        touchRouter.swallowNextGesture();
      }
    }

    return;
  }

  // ---- No new sample this tick ----
  if (!hasTouched) {
    const int raw = touchRouter.onNoSample();

    if (radialMenu.isOpen()) {
      if (raw == 1) {
        radialMenu.markFingerStillDown();
        radialMenu.tick();
      } else {
        const bool releaseConfirmed = (raw == 0);
        const auto lift = radialMenu.notifyFingerLifted(releaseConfirmed);
        if (lift == RadialMenu::LiftResult::CLOSE_MENU) {
          radialMenu.close();
          touchRouter.recordAction();
          touchRouter.swallowNextGesture();
        }
      }
      drainPendingMenuAction();
    }
    return;
  }

  // ---- A fresh gesture sample is available ----
  const int x = Hardware::touch.data.x;
  const int y = Hardware::touch.data.y;

  // Wake screen if it was off; swallow that waking touch.
  if (backlight.wakeOnTouch()) return;

  // Discard the touch that woke us from deep sleep.
  if (touchRouter.checkSwallowFirstTouch()) return;

  // Drop any touch sample that belongs to a hold/release gesture we already
  // handled through raw finger tracking. This is intentionally stronger than
  // a one-shot because the CST816S can emit delayed SINGLE_TAP samples after
  // long holds.
  if (touchRouter.checkSwallowGesture()) return;

  // BASE LOST is a modal restart state. While it is visible, block all
  // normal game/menu input. A tap restarts both counters and hides it.
  if (defeatOverlay.isActive()) {
    touchRouter.resetHold();
    if (gesture == Gesture::SINGLE_TAP) restartFromDefeatOverlay();
    return;
  }

  if (radialMenu.isOpen()) {
    radialMenu.handleTouch(x, y);
    radialMenu.tick();
    drainPendingMenuAction();
    backlight.recordActivity();
    return;
  }

  const bool isCenter = abs(x - CENTER_X) <= CENTER_TAP_HALF && abs(y - CENTER_Y) <= CENTER_TAP_HALF;

  // Touch-lock: only allow centre hold to access the menu.
  if (game.touchLocked) {
    if (isCenter) touchRouter.trackCentreHold(now);
    return;
  }

  if (isCenter && gesture == 0) {
    // Only arm the hold timer inside a tight circular zone (radius CENTER_HOLD_HALF).
    // The wider rectangular dead-zone (CENTER_TAP_HALF=32) overlaps P2's natural
    // tap zone: x/y both near 88-120 in 2P. The CST816S can omit SINGLE_TAP for
    // rapid repeated taps, leaving _hold.tracking armed. With the tighter circle,
    // those outer taps call resetHold() and the menu stays closed.
    const int dx = x - CENTER_X, dy = y - CENTER_Y;
    if (dx * dx + dy * dy <= CENTER_HOLD_HALF * CENTER_HOLD_HALF)
      touchRouter.trackCentreHold(now);
    else
      touchRouter.resetHold();
    return;
  }
  touchRouter.resetHold();

  // 1) Drop everything while we're still swallowing the finger that
  //    closed the radial menu (cleared automatically on confirmed lift).
  // 2) Cooldown is a small debounce between physically-distinct presses.
  if (touchRouter.isSwallowing()) return;
  if (!touchRouter.cooldownExpired()) return;

  // Across-each-other 2P layout: P2 occupies the top half (rotated 180),
  // P1 the bottom half. Player split is now along the Y axis, not X.
  const bool isP2Side = game.twoPlayer && (y < CENTER_Y);
  LifeCounter& target = isP2Side ? gameUi.p2() : gameUi.p1();

  switch (gesture) {
    case Gesture::SWIPE_LEFT:
      if (game.twoPlayer) {
        // 2P across: left/right is the +/- axis (matching the tap zones).
        //   P1 (bottom):      screen-LEFT = P1's left hand  = -5
        //   P2 (top/flipped): screen-LEFT = P2's right hand = +5
        target.change(isP2Side ? +5 : -5, game.twoPlayer);
        touchRouter.swallowUntilLift();
      } else {
        // 1P: left swipe arms undo.
        if (target.beginUndoPending()) {
          undoPending.arm(0);
          undoPendingOverlay.show(undoPending.player, game.twoPlayer);
          touchRouter.swallowUntilLift();
        }
      }
      break;

    case Gesture::SWIPE_RIGHT:
      if (game.twoPlayer) {
        target.change(isP2Side ? -5 : +5, game.twoPlayer);
        touchRouter.swallowUntilLift();
      }
      // 1P: right swipe is no-op.
      break;

    case Gesture::SWIPE_UP:
      if (game.twoPlayer) {
        // 2P: P2 (top/flipped) triggers undo by swiping toward screen top (their back).
        if (isP2Side && target.beginUndoPending()) {
          undoPending.arm(1);
          undoPendingOverlay.show(undoPending.player, game.twoPlayer);
          touchRouter.swallowUntilLift();
        }
      } else {
        // 1P: up = +5.
        target.change(+5, game.twoPlayer);
        touchRouter.swallowUntilLift();
      }
      break;

    case Gesture::SWIPE_DOWN:
      if (game.twoPlayer) {
        // 2P: P1 (bottom) triggers undo by swiping toward screen bottom (their back).
        if (!isP2Side && target.beginUndoPending()) {
          undoPending.arm(0);
          undoPendingOverlay.show(undoPending.player, game.twoPlayer);
          touchRouter.swallowUntilLift();
        }
      } else {
        // 1P: down = -5.
        target.change(-5, game.twoPlayer);
        touchRouter.swallowUntilLift();
      }
      break;

    case Gesture::SINGLE_TAP:
      target.tapped(x, y, game.twoPlayer);
      touchRouter.recordAction();
      break;

    default:
      break;
  }
}

// ==============================================================
// handleResetPending
//
// Called every loop tick. Uses the raw I2C touch register for
// continuous finger-down state between gesture events.
// Releasing early resets the arc to 0 but keeps pending alive;
// the timeout auto-cancels after TIMEOUT_MS of inactivity.
// ==============================================================

void handleResetPending() {
  if (!resetPending.active()) return;

  if (resetPending.timedOut()) {
    resetPending.cancel();
    resetPendingOverlay.hide();
    return;
  }

  const int raw = Hardware::readTouchFingerDownRaw();

  if (raw == 1) {
    if (!resetPending.fingerDown()) resetPending.beginHold();
    resetPendingOverlay.setProgress(resetPending.holdProgress());
    backlight.recordActivity();

    if (resetPending.holdComplete()) {
      resetPending.cancel();
      resetPendingOverlay.hide();
      gameUi.resetBoth(game.countUp);
      modeToast.show(FA_ICON_RESET, UiText::RESET, COLOR_MINUS);
      touchRouter.swallowUntilLift();
      touchRouter.swallowNextGesture();
      backlight.recordActivity();
    }
  } else if (raw == 0 && resetPending.fingerDown()) {
    // Finger lifted before full - arc empties, stay in pending. Consume the
    // possible late release tap, same as undo/menu holds.
    resetPending.releaseHold();
    resetPendingOverlay.setProgress(0.0f);
    touchRouter.swallowUntilLift();
    touchRouter.swallowNextGesture();
  }
}

// ==============================================================
// handleUndoPending
//
// Swipe arms undo; holding the centre confirms it. This mirrors the reset
// ring UX so destructive/large actions use the same interaction grammar.
// ==============================================================

void handleUndoPending() {
  if (!undoPending.active()) return;

  if (undoPending.timedOut()) {
    gameUi.clearAllUndoPending();
    undoPending.cancel();
    undoPendingOverlay.hide();
    return;
  }

  const int raw = Hardware::readTouchFingerDownRaw();

  if (raw == 1 && undoPending.fingerDown()) {
    undoPendingOverlay.setProgress(undoPending.holdProgress());
    backlight.recordActivity();

    if (undoPending.holdComplete()) {
      LifeCounter& undoTarget = (undoPending.player == 1) ? gameUi.p2() : gameUi.p1();
      undoTarget.clearUndoPending();
      undoPending.cancel();
      undoPendingOverlay.hide();
      if (undoTarget.undo()) modeToast.show(FA_ICON_UNDO, UiText::UNDO, COLOR_MENU_ORANGE);
      touchRouter.swallowUntilLift();
      touchRouter.swallowNextGesture();
      backlight.recordActivity();
    }
  } else if (raw == 0 && undoPending.fingerDown()) {
    // Finger lifted before full - arc empties, stay armed until timeout.
    // The release can still be reported as a late SINGLE_TAP by the touch IC;
    // consume that next gesture so it cannot change the life counter.
    undoPending.releaseHold();
    undoPendingOverlay.setProgress(0.0f);
    touchRouter.swallowUntilLift();
    touchRouter.swallowNextGesture();
  }
}

// ==============================================================
// handleShake
// ==============================================================

void handleShake() {
  if (!imu.update()) return;
  if (defeatOverlay.isActive()) return;  // BASE LOST uses tap-to-restart instead
  if (radialMenu.isOpen()) return;       // ignore while in menu
  if (undoPending.active()) {
    gameUi.clearAllUndoPending();
    undoPending.cancel();
    undoPendingOverlay.hide();
  }
  // Enter (or restart) reset-pending; never reset immediately.
  resetPending.arm();
  resetPendingOverlay.show();
  backlight.recordActivity();
}
