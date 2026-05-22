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
void createGameUI();
void createRadialMenuOverlay();

// ---- Per-tick handlers ----
void handleTouch();
void handleResetPending();
void handleUndoPending();
void handleShake();

void drainPendingMenuAction();

void restartFromDefeatOverlay();

void executeMenuAction(MenuAction action);
