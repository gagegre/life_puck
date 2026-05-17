// =============================================================================
// life_puck.ino
//
// Magic-the-Gathering style life counter for the Waveshare
// ESP32-S3-Touch-LCD-1.28-B (240x240 round GC9A01 + CST816S + QMI8658C).
//
// This file is intentionally minimal: just setup() and loop(). Every piece
// of behaviour lives in a dedicated module so each can be understood and
// changed in isolation. See README.md ("Code Architecture") for the file
// map.
//
// Boot flow:
//   1. Force the backlight off immediately.
//   2. Detect deep-sleep wake early and restore preferences / RTC state.
//   3. Bring up I2C / IMU / TFT / LVGL while the panel is still dark.
//   4. Decide whether this boot should show the startup intro.
//      - If yes: clear display GRAM to black, start the intro, then turn the
//        backlight on. The life-counter UI is deliberately NOT built/flushed
//        before the intro, so manual wake never flashes the counter first.
//      - If no: build the game UI, flush one clean frame while dark, then
//        turn the backlight on.
//   5. After the intro finishes, build the game UI and radial overlay.
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <lvgl.h>
#include <esp_timer.h>
#include <esp_sleep.h>

#include "Config.h"
#include "Clock.h"
#include "Hardware.h"
#include "PowerManager.h"
#include "App.h"
#include "StartupIntro.h"

StartupIntro* startupIntro = nullptr;

static bool gWokeFromDeepSleep = false;
static bool gGameUiPreparedForIntroReveal = false;
static bool gPendingPostIntroReset = false;

static int resetStartValueForIntro(bool countUpMode, int baseLife);

// Decide what the device is booting *because of*. Called once at the top of
// setup() after we've already latched the ESP32 wake cause and after NVS has
// been read (so rtcState.lastSleepReason is meaningful when we slept).
//
//   - No wake-from-sleep                  -> ColdBoot            (intro plays)
//   - Woke from sleep, reason=Manual      -> WakeFromManualSleep (intro plays)
//   - Woke from sleep, reason=IdleTimeout -> WakeFromIdleSleep   (intro skipped)
//   - Woke from sleep, reason=None        -> WakeFromIdleSleep   (defensive
//                                            fallback: treat unknown wakes
//                                            as silent, matching the
//                                            previous behavior).
static StartupIntro::BootReason computeBootReason(bool wokeFromDeepSleep) {
  if (!wokeFromDeepSleep) return StartupIntro::BootReason::ColdBoot;
  switch (rtcState.lastSleepReason) {
    case SleepReason::Manual:
      return StartupIntro::BootReason::WakeFromManualSleep;
    case SleepReason::IdleTimeout:
      return StartupIntro::BootReason::WakeFromIdleSleep;
    case SleepReason::None:
      return StartupIntro::BootReason::WakeFromIdleSleep;
  }
  return StartupIntro::BootReason::WakeFromIdleSleep;
}

static void buildGameUiForBoot(bool wokeFromDeepSleep, bool animateInitialReset = true) {
  createGameUI();
  battery.forceRefresh();

  // Restore life values now that the labels exist.
  if (wokeFromDeepSleep) {
    gameUi.restoreValues(rtcState.life, rtcState.life2);

    // Re-apply the final 2P/1P layout after restoring text. The restored
    // value can change the label width, which matters for the rotated P2
    // transform and its alignment.
    if (game.twoPlayer)
      gameUi.enterTwoPlayer();
    else
      gameUi.exitTwoPlayer();
  } else if (animateInitialReset) {
    gameUi.resetBoth(game.countUp, game.twoPlayer);
  } else {
    const int p1Start = resetStartValueForIntro(game.countUp, game.baseLife1);
    const int p2Start = resetStartValueForIntro(game.countUp, game.baseLife2);
    gameUi.restoreValues(p1Start, p2Start);

    if (game.twoPlayer)
      gameUi.enterTwoPlayer();
    else
      gameUi.exitTwoPlayer();
  }

  // Build the radial overlay only after the main game UI exists, so any
  // hidden overlay objects are layered above the counter without contributing
  // an old retained frame during wake.
  createRadialMenuOverlay();
}

static int resetStartValueForIntro(bool countUpMode, int baseLife) {
  // Match LifeCounter::reset(...) start value:
  // HP/count-down mode: show 0 first, then count up to base life.
  // Damage/count-up mode: show base life first, then count down to 0.
  return countUpMode ? baseLife : LIFE_MIN;
}

void prepareGameUiForIntroReveal(void* userData) {
  (void)userData;

  if (gGameUiPreparedForIntroReveal) return;
  gGameUiPreparedForIntroReveal = true;

  // During the crosshair/reveal phase, show the reset-start value only.
  // Do NOT start the reset animation yet; it begins after the intro is gone.
  buildGameUiForBoot(gWokeFromDeepSleep, false);

  const int p1Start = resetStartValueForIntro(game.countUp, game.baseLife1);
  const int p2Start = resetStartValueForIntro(game.countUp, game.baseLife2);
  gameUi.restoreValues(p1Start, p2Start);

  if (game.twoPlayer)
    gameUi.enterTwoPlayer();
  else
    gameUi.exitTwoPlayer();

  gPendingPostIntroReset = true;

  lv_obj_invalidate(lv_screen_active());
}

void onStartupIntroFinished(void* userData) {
  startupIntro = nullptr;

  // If the intro was skipped before the reveal phase, the game UI was not
  // prepared yet. Build it now as fallback.
  if (!gGameUiPreparedForIntroReveal) {
    prepareGameUiForIntroReveal(userData);
  }

  if (gPendingPostIntroReset) {
    gPendingPostIntroReset = false;

    // Crosshair is gone now. Start the normal reset/count animation here,
    // same path/speed as other UI reset transitions.
    gameUi.resetBoth(game.countUp, game.twoPlayer);
  }

  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(Hardware::lvDisplay);

  analogWrite(PIN_LCD_BACKLIGHT, backlight.level());

  Hardware::touch.begin();
  backlight.recordActivity();

  Serial.println("Ready.");
}

void setup() {
  // First instruction in setup(): force the panel light dark before Serial,
  // WiFi shutdown, I2C, TFT init, or LVGL can touch anything. This reduces the
  // visible boot/wake flash window to the hardware-reset interval only.
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACKLIGHT, LOW);
  analogWrite(PIN_LCD_BACKLIGHT, 0);

  Serial.begin(115200);

  // Disable radios up front. We never use them, and they cost ~20-30 mA.
  WiFi.mode(WIFI_OFF);
  btStop();

  // Keep the LCD dark until the first clean LVGL frame has been flushed.
  // Some display/backlight init paths briefly drive the pin high otherwise,
  // which looks like a full-bright wake flash.
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACKLIGHT, LOW);
  analogWrite(PIN_LCD_BACKLIGHT, 0);

  // Detect wake reason early. On deep-sleep wake the LCD controller can still
  // contain the clean life-counter frame we wrote before sleeping. Do not blank
  // that retained frame, or the user sees: counter -> black -> counter.
  gWokeFromDeepSleep = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0);

  const bool wokeFromDeepSleep = gWokeFromDeepSleep;

  // Load persisted preferences from NVS before touching the backlight, so wake
  // can keep roughly the intended brightness instead of briefly going black.
  nvm.begin();
  const uint8_t savedBacklight = nvm.getBrightness();
  backlight.setLevel(savedBacklight, false);
  battery.setMode(nvm.getBatteryMode());
  battery.setShowPercent(nvm.getBatteryShowPct());

  // Inject the deep-sleep callback so the Backlight module can fire it after
  // the staged idle-timeout chain reaches DEEP_SLEEP_MS, without taking a
  // hard dependency on PowerManager. We pick the *idle* variant explicitly:
  // the next boot will see SleepReason::IdleTimeout and skip the intro.
  backlight.setIdleSleepCallback(&PowerManager::deepSleepIdle);

  // ADC for battery: 12-bit with 11 dB attenuation to read the full
  // ~0..3.3 V divider range linearly.
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);

  // I2C bus is shared between the touch panel and the IMU.
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  imu.begin();

  Hardware::tft.init();
  // TFT_eSPI / panel init may briefly reconfigure or drive TFT_BL. Re-assert
  // OFF immediately after init before doing any visible drawing.
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACKLIGHT, LOW);
  analogWrite(PIN_LCD_BACKLIGHT, 0);

  Hardware::tft.setRotation(0);
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACKLIGHT, LOW);
  analogWrite(PIN_LCD_BACKLIGHT, 0);
  if (!wokeFromDeepSleep) {
    // Fresh boot only; don't blank the retained wake frame.
    Hardware::tft.fillScreen(TFT_BLACK);
  }

  // Restore RTC state if waking from deep sleep, otherwise load NVS base-life.
  if (wokeFromDeepSleep) {
    game.countUp = rtcState.countUp;
    game.twoPlayer = rtcState.twoPlayer;
    game.touchLocked = rtcState.touchLocked;
    game.baseLife1 = rtcState.baseLife1;
    game.baseLife2 = rtcState.baseLife2;
    // Drop the touch that woke us; one-shot flag avoids the
    // millis()-near-zero underflow that the previous version had.
    touchRouter.requestSwallowFirstTouch();
  } else {
    game.baseLife1 = nvm.getBaseLife1();
    game.baseLife2 = nvm.getBaseLife2();
  }

  // ---- LVGL bring-up ----
  lv_init();
  Hardware::lvDisplay = lv_display_create(SCREEN_W, SCREEN_H);
  lv_display_set_color_format(Hardware::lvDisplay, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(Hardware::lvDisplay, Hardware::displayFlush);
  lv_display_set_buffers(
      Hardware::lvDisplay, Hardware::drawBuffer, nullptr, sizeof(Hardware::drawBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);

  // LVGL needs a regular tick. We service it from an esp_timer task instead
  // of spinning in loop() so timing stays steady even when the loop is busy.
  const esp_timer_create_args_t tickArgs = {
      .callback = &Hardware::lvTickTask, .arg = nullptr, .dispatch_method = ESP_TIMER_TASK, .name = "lv_tick"};
  esp_timer_handle_t tickHandle;
  esp_timer_create(&tickArgs, &tickHandle);
  esp_timer_start_periodic(tickHandle, LVGL_TICK_INTERVAL_MS * 1000UL);

#if ENABLE_STARTUP_INTRO
  // Decide before building/flushing the life-counter UI. If the intro will
  // play, the counter must not be drawn first, otherwise manual wake shows:
  // retained/counter frame -> intro. That is the bad flash.
  const StartupIntro::BootReason bootReason = computeBootReason(wokeFromDeepSleep);
  if (StartupIntro::shouldShowFor(bootReason)) {
    // Write black directly to display GRAM via SPI before enabling the
    // backlight.
    //
    // On wake from manual deep sleep the GC9A01 can retain the previous
    // life-counter frame. tft.fillScreen() is a synchronous full-panel SPI
    // write, so the display is guaranteed black before PWM enables the light.
    Hardware::tft.fillScreen(TFT_BLACK);

    gGameUiPreparedForIntroReveal = false;

    startupIntro =
        StartupIntro::start(lv_screen_active(), onStartupIntroFinished, nullptr, prepareGameUiForIntroReveal);

    // Push the intro's first frame while still dark. Then the first visible
    // thing after manual wake is the intro, not the life counter.
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(Hardware::lvDisplay);

    analogWrite(PIN_LCD_BACKLIGHT, backlight.level());
    return;
  }
#endif

  // Silent boot path: either the intro is compiled out, or we're waking
  // from an idle-timeout sleep. Build the game UI and flush one clean frame
  // while the light is still off.
  buildGameUiForBoot(wokeFromDeepSleep);

  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACKLIGHT, LOW);
  analogWrite(PIN_LCD_BACKLIGHT, 0);

  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(Hardware::lvDisplay);

  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACKLIGHT, LOW);
  analogWrite(PIN_LCD_BACKLIGHT, 0);

  analogWrite(PIN_LCD_BACKLIGHT, backlight.level());

  Hardware::touch.begin();
  backlight.recordActivity();

  Serial.println("Ready.");
}

#if ENABLE_STARTUP_INTRO
static bool handleStartupIntroLoop() {
  if (startupIntro == nullptr) return false;

  // Hold-to-skip: poll the raw finger-down register at a modest cadence.
  // Hardware::readTouchFingerDownRaw() reads CST816S register 0x02 directly
  // over I2C — the same mechanism the radial menu uses for continuous hold
  // tracking. It works before Hardware::touch.begin() has been called because
  // Wire is already up from setup().
  //
  // We require TSkipHold ms of uninterrupted contact to skip, so the brief
  // waking touch that triggers the deep-sleep wake interrupt can never
  // accidentally fire the skip.
  static uint32_t lastIntroTouchPollAt = 0;

  if (Clock::tick(lastIntroTouchPollAt, 20)) {
    const int rawDown = Hardware::readTouchFingerDownRaw();

    if (rawDown == 1) {
      startupIntro->notifyHoldStart();
    } else if (rawDown == 0) {
      startupIntro->notifyHoldEnd();
    }
    // rawDown == -1 is an I2C glitch, leave the hold state unchanged.
  }

  lv_timer_handler();
  delay(1);
  return true;
}
#endif

// =============================================================================
// loop()
//
// The main-loop responsibilities are intentionally narrow: poll input,
// run per-tick handlers, push LVGL forward. Everything else lives in the
// modules.
// =============================================================================

void loop() {
#if ENABLE_STARTUP_INTRO
  if (handleStartupIntroLoop()) return;
#endif

  handleTouch();
  handleResetPending();
  flashMgr.update();

  static uint32_t lastImuAt = 0;
  if (Clock::tick(lastImuAt, IMU_POLL_MS)) handleShake();

  radialMenu.tick();
  // Auto-commits triggered from idleTick (e.g. brightness / base-life
  // selectors after a brief lift) queue a pending action and close the
  // menu directly. Drain so the toast and NVM persistence fire in the
  // same loop iteration instead of next-tick.
  drainPendingMenuAction();
  modeToast.update();
  if (!radialMenu.isOpen()) gameUi.updateDeltas(Clock::now());

  // ---- Defeat overlay ----
  // Cancel early if the player has been healed out of the defeat state
  // (distance back > 0). Otherwise drive its animation tick.
  if (defeatOverlay.isActive()) {
    LifeCounter& victim = (defeatOverlay.player() == 1) ? gameUi.p2() : gameUi.p1();
    if (!victim.isDefeated()) {
      defeatOverlay.cancel();
      gameUi.setCountersVisible(true, game.twoPlayer);
    } else {
      defeatOverlay.update(Clock::now());
    }
  }

  // Cancel undo-pending state after timeout with no confirmation.
  if (undoPending.timedOut()) {
    gameUi.clearAllUndoPending();
    undoPending.cancel();
  }

  if (battery.mode() != BatteryMode::HIDE) battery.update();
  flashMgr.setContracted(battery.shouldShow());

  if (!radialMenu.isOpen()) backlight.checkTimeout();

  lv_timer_handler();
  delay(LOOP_DELAY_MS);
}
