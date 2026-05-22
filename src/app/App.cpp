// App.cpp
//
// Owns the singleton instances and the app-level functions that the
// .ino calls from setup()/loop().

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
  battery.begin(scr);

  // Flash feedback arcs and life label (both above the battery arc).
  flashMgr.begin(scr);
  gameUi.begin(scr, &flashMgr, &battery);
  modeToast.begin(scr);
  resetPendingOverlay.begin(scr);
  undoPendingOverlay.begin(scr);
  defeatOverlay.begin(scr, scr);

  // Wire the defeat callback so the counter hitting distance=0 fires the
  // full-screen overlay. Plain function pointer to avoid std::function on
  // a 320 KB-RAM target.
  auto defeatCb = +[]() {
    gameUi.clearUndoPending();
    undoPending.cancel();
    undoPendingOverlay.hide();
    resetPending.cancel();
    resetPendingOverlay.hide();
    gameUi.hideBaseReveal();
    gameUi.setCounterVisible(false);
    defeatOverlay.show();
  };
  gameUi.p().setDefeatCallback(defeatCb);

  lv_obj_move_foreground(gameUi.p().lvObj());

  gameUi.setBaseLife(game.baseLife);
  gameUi.setCountUp(game.countUp);
}

void createRadialMenuOverlay() {
  // Build this only after the first life-counter frame was flushed.
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

    case MenuAction::COUNT_DIRECTION:
      game.countUp = !game.countUp;
      gameUi.setCountUp(game.countUp);
      gameUi.reset(game.countUp);
      radialMenu.onGameStateChanged();
      modeToast.show(iconForAction(MenuAction::COUNT_DIRECTION, game),
                     game.countUp ? UiText::COUNT_UP : UiText::COUNT_DOWN,
                     COLOR_MENU_ORANGE);
      break;

    case MenuAction::COUNT_DOWN:
      if (game.countUp) {
        game.countUp = false;
        gameUi.setCountUp(false);
        gameUi.reset(game.countUp);
      }
      radialMenu.onGameStateChanged();
      modeToast.show(FA_ICON_COUNT_DOWN, UiText::COUNT_DOWN, COLOR_MENU_ORANGE);
      break;

    case MenuAction::COUNT_UP:
      if (!game.countUp) {
        game.countUp = true;
        gameUi.setCountUp(true);
        gameUi.reset(game.countUp);
      }
      radialMenu.onGameStateChanged();
      modeToast.show(FA_ICON_COUNT_UP, UiText::COUNT_UP, COLOR_MENU_ORANGE);
      break;

    case MenuAction::BASE_SELECTOR_COMMIT: {
      // BaseSelectorView has already written the chosen value into
      // game.baseLife; persist and apply now.
      gameUi.setBaseLife(game.baseLife);

      // Only after explicit confirm: reset the counter to the new full base life.
      gameUi.reset(game.countUp);

      nvm.setBaseLife(game.baseLife);
      radialMenu.onGameStateChanged();
      char buf[24];
      snprintf(buf, sizeof(buf), UiText::BASE_LIFE_FMT_1P, game.baseLife);
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
// ==============================================================

void drainPendingMenuAction() {
  const MenuAction a = radialMenu.takePendingAction();
  if (a == MenuAction::NONE) return;

  // Sub-view commits close the menu; top-level cycles stay open.
  const bool closeActions[] = {a == MenuAction::SLEEP,
                               a == MenuAction::SLEEP_OFF,
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
    // follow-up SINGLE_TAP for that same press and bump the life counter.
    touchRouter.swallowUntilLift();
    touchRouter.swallowNextGesture();
  }
  executeMenuAction(a);
  // Save brightness/battery prefs whenever a sub-view committed them.
  nvm.setBrightness(backlight.level());
  nvm.setBatteryMode(battery.mode());
  nvm.setBatteryShowPct(battery.isShowingPercent());
}

void restartFromDefeatOverlay() {
  defeatOverlay.cancel();
  gameUi.setCounterVisible(true);
  gameUi.reset(game.countUp);
  undoPending.cancel();
  undoPendingOverlay.hide();
  gameUi.clearUndoPending();
  resetPending.cancel();
  resetPendingOverlay.hide();
  modeToast.show(FA_ICON_RESET, UiText::RESET, COLOR_MINUS);
  touchRouter.swallowUntilLift();
  backlight.recordActivity();
}

// ==============================================================
// handleTouch
//
// Input priority each tick (1P-only):
//   1. Centre-hold soft timer elapsed             -> open radial menu
//   2. Outside-centre long-hold elapsed (no other -> show OF XY label
//      armed confirmation, finger outside centre)
//   3. Reset confirmation pending                 -> any gesture cancels it
//   4. Undo confirmation pending                  -> centre hold confirms
//   5. No fresh gesture                           -> radial-menu tick / lift
//   6. Backlight off / first wake touch           -> swallow
//   7. Radial menu open                           -> radialMenu.handleTouch
//   8. Centre dead-zone (no gesture)              -> track centre hold
//   9. Outside dead-zone (no gesture)             -> track outside hold
//  10. Touch-locked                               -> centre-hold only
//  11. Cooldown / per-session latch               -> drop redundant samples
//  12. Otherwise                                  -> tap/swipe to LifeCounter
//
// Any tap/swipe also drops the OF XY label so the reveal is single-shot
// and never lingers across distinct user actions.
// ==============================================================

void handleTouch() {
  const uint32_t now = Clock::now();

  // ---- 1. Centre-hold opens the radial menu ----
  // Armed confirmation states own the centre hold, so they must not
  // accidentally trigger this.
  if (!radialMenu.isOpen() && !resetPending.active() && !undoPending.active() &&
      !defeatOverlay.isActive() && touchRouter.holdComplete(now)) {
    gameUi.hideBaseReveal();  // can't coexist with the menu
    radialMenu.show();
    touchRouter.markHoldOpenedMenu();
    touchRouter.recordAction();
    backlight.recordActivity();
  }

  // ---- 2. Outside-centre hold reveals OF XY ----
  // Strict: only fires when nothing else is armed and the menu is closed.
  // This prevents the gesture from sneaking in during a reset/undo arm or
  // mid-radial-menu interaction. The same swallow handshake the menu uses
  // is applied here so the release does not leak as a SINGLE_TAP.
  if (!radialMenu.isOpen() && !resetPending.active() && !undoPending.active() &&
      !defeatOverlay.isActive() && touchRouter.outsideHoldComplete(now)) {
    touchRouter.markOutsideHoldFired();
    gameUi.showBaseReveal();
    touchRouter.swallowUntilLift();
    touchRouter.swallowNextGesture();
    backlight.recordActivity();
  }

  const bool hasTouched = Hardware::touch.available();
  const int gesture = hasTouched ? Hardware::touch.data.gestureID : 0;

  // ---- 3. Reset-pending: block normal input ----
  if (resetPending.active()) {
    touchRouter.resetHold();
    touchRouter.resetOutsideHold();
    backlight.recordActivity();
    if (hasTouched && gesture != 0) {
      resetPending.cancel();
      resetPendingOverlay.hide();
    }
    return;
  }

  // ---- 4. Undo-pending: block normal input ----
  if (undoPending.active()) {
    touchRouter.resetHold();
    touchRouter.resetOutsideHold();
    backlight.recordActivity();

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
        gameUi.clearUndoPending();
        undoPending.cancel();
        undoPendingOverlay.hide();
        touchRouter.swallowUntilLift();
        touchRouter.swallowNextGesture();
      }
    }

    return;
  }

  // ---- 5. No new sample this tick ----
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

  // ---- 6/7. Fresh gesture sample is available ----
  const int x = Hardware::touch.data.x;
  const int y = Hardware::touch.data.y;

  // Wake screen if it was off; swallow that waking touch.
  if (backlight.wakeOnTouch()) return;

  // Discard the touch that woke us from deep sleep.
  if (touchRouter.checkSwallowFirstTouch()) return;

  // Drop any touch sample that belongs to a hold/release gesture we already
  // handled through raw finger tracking.
  if (touchRouter.checkSwallowGesture()) return;

  // ---- BASE LOST modal: tap restarts, everything else is blocked ----
  if (defeatOverlay.isActive()) {
    touchRouter.resetHold();
    touchRouter.resetOutsideHold();
    if (gesture == Gesture::SINGLE_TAP) restartFromDefeatOverlay();
    return;
  }

  // ---- Radial menu owns the gesture stream while it's open ----
  if (radialMenu.isOpen()) {
    radialMenu.handleTouch(x, y);
    radialMenu.tick();
    drainPendingMenuAction();
    backlight.recordActivity();
    return;
  }

  // Any deliberate gesture cancels any active OF XY reveal so the label
  // never sits on top of fresh game input.
  const bool isGestureSample = (gesture != 0);

  const bool isCenter = abs(x - CENTER_X) <= CENTER_TAP_HALF && abs(y - CENTER_Y) <= CENTER_TAP_HALF;

  // ---- Touch-lock: only allow centre hold ----
  if (game.touchLocked) {
    if (isCenter) {
      touchRouter.trackCentreHold(now);
    } else {
      touchRouter.resetHold();
    }
    touchRouter.resetOutsideHold();
    return;
  }

  // ---- Idle finger inside / outside the centre dead-zone ----
  // gesture == 0 means "finger is here but no swipe/tap yet". We use this
  // window to arm one of the two hold trackers, never both.
  if (gesture == 0) {
    const int dx = x - CENTER_X, dy = y - CENTER_Y;
    const bool inCentreHoldCircle = (dx * dx + dy * dy) <= (CENTER_HOLD_HALF * CENTER_HOLD_HALF);
    if (inCentreHoldCircle) {
      touchRouter.trackCentreHold(now);
    } else if (isCenter) {
      // Inside the wider rectangular centre but outside the strict hold
      // circle: do not arm the menu hold (avoids false menu opens from
      // edge-of-centre rests), and do not arm the outside hold either
      // (the finger is still effectively "in the middle").
      touchRouter.resetHold();
      touchRouter.resetOutsideHold();
    } else {
      // Genuinely outside the centre dead-zone: arm OF XY hold tracker.
      touchRouter.resetHold();
      touchRouter.trackOutsideHold(now);
    }
    return;
  }

  // ---- A real gesture has arrived: drop any pending holds ----
  touchRouter.resetHold();
  touchRouter.resetOutsideHold();
  if (isGestureSample) gameUi.hideBaseReveal();

  if (touchRouter.isSwallowing()) return;
  if (!touchRouter.cooldownExpired()) return;

  switch (gesture) {
    case Gesture::SWIPE_LEFT:
      // 1P: left swipe arms undo.
      if (gameUi.p().beginUndoPending()) {
        undoPending.arm();
        undoPendingOverlay.show();
        touchRouter.swallowUntilLift();
      }
      break;

    case Gesture::SWIPE_RIGHT:
      // 1P: no-op.
      break;

    case Gesture::SWIPE_UP:
      gameUi.p().change(+5);
      touchRouter.swallowUntilLift();
      break;

    case Gesture::SWIPE_DOWN:
      gameUi.p().change(-5);
      touchRouter.swallowUntilLift();
      break;

    case Gesture::SINGLE_TAP:
      gameUi.p().tapped(y);
      touchRouter.recordAction();
      break;

    default:
      break;
  }
}

// ==============================================================
// handleResetPending
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
      gameUi.reset(game.countUp);
      modeToast.show(FA_ICON_RESET, UiText::RESET, COLOR_MINUS);
      touchRouter.swallowUntilLift();
      touchRouter.swallowNextGesture();
      backlight.recordActivity();
    }
  } else if (raw == 0 && resetPending.fingerDown()) {
    resetPending.releaseHold();
    resetPendingOverlay.setProgress(0.0f);
    touchRouter.swallowUntilLift();
    touchRouter.swallowNextGesture();
  }
}

// ==============================================================
// handleUndoPending
// ==============================================================

void handleUndoPending() {
  if (!undoPending.active()) return;

  if (undoPending.timedOut()) {
    gameUi.clearUndoPending();
    undoPending.cancel();
    undoPendingOverlay.hide();
    return;
  }

  const int raw = Hardware::readTouchFingerDownRaw();

  if (raw == 1 && undoPending.fingerDown()) {
    undoPendingOverlay.setProgress(undoPending.holdProgress());
    backlight.recordActivity();

    if (undoPending.holdComplete()) {
      gameUi.p().clearUndoPending();
      undoPending.cancel();
      undoPendingOverlay.hide();
      if (gameUi.p().undo()) modeToast.show(FA_ICON_UNDO, UiText::UNDO, COLOR_MENU_ORANGE);
      touchRouter.swallowUntilLift();
      touchRouter.swallowNextGesture();
      backlight.recordActivity();
    }
  } else if (raw == 0 && undoPending.fingerDown()) {
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
  if (defeatOverlay.isActive()) return;
  if (radialMenu.isOpen()) return;
  if (undoPending.active()) {
    gameUi.clearUndoPending();
    undoPending.cancel();
    undoPendingOverlay.hide();
  }
  gameUi.hideBaseReveal();
  // Enter (or restart) reset-pending; never reset immediately.
  resetPending.arm();
  resetPendingOverlay.show();
  backlight.recordActivity();
}
