// PowerManager.cpp

#include "PowerManager.h"
#include "App.h"
#include "Config.h"
#include "Hardware.h"

#include <esp_sleep.h>
#include <Arduino.h>
#include <lvgl.h>

namespace PowerManager {
namespace {

void flushNow() {
  if (!Hardware::lvDisplay) return;

  lv_obj_invalidate(lv_screen_active());
  lv_timer_handler();
  lv_refr_now(Hardware::lvDisplay);
  lv_timer_handler();
}

void forceBacklightPinOff() {
  // Keep this as raw pin/PWM writes so it does not alter Backlight's logical
  // state. Some display init paths briefly reconfigure the backlight pin, so
  // PowerManager and setup() can re-assert OFF without marking the app asleep.
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACKLIGHT, LOW);
  analogWrite(PIN_LCD_BACKLIGHT, 0);
}

void showSleepHint() {
  // Reuse the normal mode-toast overlay so sleep confirmation has the same
  // visual language as the rest of the radial-menu confirmations. For sleep,
  // the toast requests a fully opaque backdrop so the life counter disappears
  // instead of showing through the dim layer.
  gameUi.setCounterVisible(false);
  modeToast.show(FA_ICON_SLEEP_ZZZ, UiText::SLEEP, COLOR_MENU_BLUE, true);

  flushNow();
  backlight.on();
  delay(650);
}

void restoreCleanFrameInvisible() {
  backlight.off();
  forceBacklightPinOff();
  delay(10);

  // Always leave the LCD RAM in its clean life-counter state before entering
  // deep sleep. This prevents waking to the sleep icon/menu frame.
  if (radialMenu.isOpen()) {
    radialMenu.close();
  } else {
    gameUi.showAfterMenu();
  }

  flushNow();
  delay(30);
}

// Shared implementation. Both public entry points funnel through here so the
// only thing that varies between manual and idle sleep is the reason we
// stamp onto rtcState (which the boot path reads on wake).
void deepSleep(SleepReason reason) {
  // Store state first, while all objects are still alive and unchanged.
  rtcState.life = gameUi.p().getValue();
  rtcState.countUp = game.countUp;
  rtcState.touchLocked = game.touchLocked;
  rtcState.baseLife = gameUi.p().getBaseLife();
  rtcState.lastSleepReason = reason;

  // Show a short explicit confirmation before sleeping so the user knows the
  // action worked. Then hide the transition and save a clean wake frame.
  showSleepHint();
  restoreCleanFrameInvisible();

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_TOUCH_INT, 0);
  esp_deep_sleep_start();
}

}  // namespace

void deepSleepManual() {
  deepSleep(SleepReason::Manual);
}

void deepSleepIdle() {
  deepSleep(SleepReason::IdleTimeout);
}

}  // namespace PowerManager
