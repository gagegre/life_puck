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
//   1. Detect deep-sleep wake early. If we woke from sleep the LCD
//      controller still holds the last clean life-counter frame; we
//      deliberately do not blank it so the user never sees a black flash.
//   2. Load NVS preferences (brightness, battery display mode) before
//      touching the backlight, so wake comes back at the user's chosen
//      brightness.
//   3. Bring up I2C / IMU / TFT / LVGL.
//   4. Restore RTC state on wake, or NVS base-life on fresh boot.
//   5. Build the always-visible game UI, flush one synchronous frame, then
//      build the (hidden) radial-menu overlay. Doing the overlay AFTER the
//      first life-counter frame is what prevents a wake-flash of the old
//      menu image that was on screen before sleeping.
//   6. Turn the backlight on at the saved brightness, start touch, mark
//      activity so the idle chain starts from now.
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

// Decide what the device is booting *because of*. Called once at the top of
// setup() after we've already latched the ESP32 wake cause and after NVS has
// been read (so rtcState.lastSleepReason is meaningful when we slept).
//
//   - No wake-from-sleep        -> ColdBoot           (intro plays)
//   - Woke from sleep, reason=Manual      -> WakeFromManualSleep (intro plays)
//   - Woke from sleep, reason=IdleTimeout -> WakeFromIdleSleep   (intro skipped)
//   - Woke from sleep, reason=None        -> WakeFromIdleSleep   (defensive
//                                            fallback: treat unknown wakes
//                                            as silent, matching the
//                                            previous behavior).
static StartupIntro::BootReason computeBootReason(bool wokeFromDeepSleep) {
  if (!wokeFromDeepSleep) return StartupIntro::BootReason::ColdBoot;
  switch (rtcState.lastSleepReason) {
    case SleepReason::Manual:      return StartupIntro::BootReason::WakeFromManualSleep;
    case SleepReason::IdleTimeout: return StartupIntro::BootReason::WakeFromIdleSleep;
    case SleepReason::None:        return StartupIntro::BootReason::WakeFromIdleSleep;
  }
  return StartupIntro::BootReason::WakeFromIdleSleep;
}

void onStartupIntroFinished(void* userData) {
  startupIntro = nullptr;

  createRadialMenuOverlay();

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
  gWokeFromDeepSleep =
    (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0);

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
  lv_display_set_buffers(Hardware::lvDisplay,
                         Hardware::drawBuffer, nullptr,
                         sizeof(Hardware::drawBuffer),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  // LVGL needs a regular tick. We service it from an esp_timer task instead
  // of spinning in loop() so timing stays steady even when the loop is busy.
  const esp_timer_create_args_t tickArgs = {
    .callback = &Hardware::lvTickTask,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "lv_tick"
  };
  esp_timer_handle_t tickHandle;
  esp_timer_create(&tickArgs, &tickHandle);
  esp_timer_start_periodic(tickHandle, LVGL_TICK_INTERVAL_MS * 1000UL);

  // Build only the always-visible life-counter layer first. The radial
  // overlay is built AFTER the first frame so wake-from-sleep never
  // flashes the menu/selection that was visible before sleeping.
  createGameUI();
  battery.forceRefresh();

  // Restore life values now that the labels exist.
  if (wokeFromDeepSleep) {
    gameUi.restoreValues(rtcState.life, rtcState.life2);
    // Re-apply the final 2P/1P layout after restoring text. The restored
    // value can change the label width, which matters for the rotated P2
    // transform and its alignment.
    if (game.twoPlayer) gameUi.enterTwoPlayer();
    else gameUi.exitTwoPlayer();
  } else {
    gameUi.resetBoth(game.countUp, /*twoPlayerMode=*/true);
  }

  // Force a synchronous first-frame flush while the light is still off.
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACKLIGHT, LOW);
  analogWrite(PIN_LCD_BACKLIGHT, 0);

  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(Hardware::lvDisplay);

  // Re-assert OFF once more after the flush. The next write is the intentional
  // restore to the saved brightness.
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACKLIGHT, LOW);
  analogWrite(PIN_LCD_BACKLIGHT, 0);

#if ENABLE_STARTUP_INTRO
  // Ask the intro itself whether it wants to play this boot. It says yes on
  // cold boot and on wake from a manual sleep; no on wake from an idle-
  // timeout sleep. This is the only place the boot path looks at the boot
  // reason — everything past the intro behaves the same regardless.
  const StartupIntro::BootReason bootReason = computeBootReason(wokeFromDeepSleep);
  if (StartupIntro::shouldShowFor(bootReason)) {
    startupIntro = StartupIntro::start(
      lv_screen_active(),
      onStartupIntroFinished,
      nullptr);

    analogWrite(PIN_LCD_BACKLIGHT, backlight.level());
    return;
  }
#endif

  // Silent boot path: either the intro is compiled out, or we're waking
  // from an idle-timeout sleep. Behave exactly like the original code did
  // for any deep-sleep wake: just bring the radial overlay up and finish.
  createRadialMenuOverlay();

  analogWrite(PIN_LCD_BACKLIGHT, backlight.level());

  Hardware::touch.begin();
  backlight.recordActivity();

  Serial.println("Ready.");
}

// =============================================================================
// loop()
//
// The main-loop responsibilities are intentionally narrow: poll input,
// run per-tick handlers, push LVGL forward. Everything else lives in the
// modules.
// =============================================================================

void loop() {
#if ENABLE_STARTUP_INTRO
  if (startupIntro != nullptr) {
    lv_timer_handler();
    delay(LOOP_DELAY_MS);
    return;
  }
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
    LifeCounter& victim =
      (defeatOverlay.player() == 1) ? gameUi.p2() : gameUi.p1();
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
