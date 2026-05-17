// App.h
//
// Glue layer: declares the singleton instances and the top-level
// app-logic functions that wire everything together. The .ino is then
// just setup() and loop() over these declarations.
//
// Definitions live in App.cpp. PowerManager.cpp also includes this
// header to reach into gameUi, game, backlight, and radialMenu for
// state-save before deep sleep.

#pragma once

#include "Config.h"
#include "Imu.h"
#include "NvmSettings.h"
#include "Backlight.h"
#include "Battery.h"
#include "FlashManager.h"
#include "GameUi.h"
#include "ModeToast.h"
#include "RadialMenu.h"
#include "ResetPending.h"
#include "DefeatOverlay.h"
#include "UndoPending.h"
#include "TouchRouter.h"

// ---- Persistent (RTC) state ----
//
// Held in RTC memory across deep sleep. Cleared on full power-off.
// Definition is in App.cpp with the RTC_DATA_ATTR attribute.
extern PersistentState rtcState;

// ---- Singletons ----
extern IMU imu;
extern NvmSettings nvm;
extern Battery battery;
extern Backlight backlight;
extern FlashManager flashMgr;
extern GameUi gameUi;
extern ModeToast modeToast;
extern RadialMenu radialMenu;
extern ResetPendingOverlay resetPendingOverlay;
extern ResetPending resetPending;
extern DefeatOverlay defeatOverlay;
extern UndoPending undoPending;
extern UndoPendingOverlay undoPendingOverlay;
extern TouchRouter touchRouter;
extern GameState game;

// ---- UI construction ----
//
// Build the always-visible game-counter layer (battery arc, flash
// arcs, life labels, toast/overlay scaffolding) and wire the defeat
// callback that fires the BASE LOST modal.
void createGameUI();

// Build the (initially hidden) radial-menu overlay. Done AFTER the
// first life-counter frame is flushed so a deep-sleep wake never
// flashes the old menu image.
void createRadialMenuOverlay();

// ---- Per-tick handlers ----
//
// Called from loop(). See App.cpp for behaviour details.
void handleTouch();
void handleResetPending();
void handleUndoPending();
void handleShake();

// Drains a pending action queued by RadialMenu::fireAction(). Closes
// the menu first when the action is a sub-view commit, then dispatches
// the action and persists any related NVS prefs.
void drainPendingMenuAction();

// Restores the game out of the BASE LOST overlay state.
void restartFromDefeatOverlay();

// Dispatch one MenuAction. Pulled out of the touch path so other call
// sites (e.g. drainPendingMenuAction) can also fire actions.
void executeMenuAction(MenuAction action);
