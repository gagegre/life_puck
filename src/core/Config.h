// Config.h
//
// Single header for *all* compile-time configuration: pin assignments,
// display geometry, game rules, animation thresholds, timing, colours,
// gesture IDs, IMU constants, battery constants, font/icon declarations,
// UI text strings, and the few enums/structs that several modules share.
//
// Anything truly runtime-tunable lives elsewhere (NvmSettings); everything
// here is `constexpr` and known at compile time.
//
// This file has no project-level dependencies — every other module
// includes it but it includes only third-party / system headers.

#pragma once

#define ENABLE_STARTUP_INTRO 1

#include <lvgl.h>
#include <Arduino.h>
#include <math.h>

// ==============================================================
// External custom font assets (provided as .c files in the sketch folder)
// ==============================================================

LV_FONT_DECLARE(life_font_72);
LV_FONT_DECLARE(life_font_96);
LV_FONT_DECLARE(font_awesome_icons);

// ==============================================================
// Font Awesome glyphs used in the UI. UTF-8 byte sequences for the
// private-use codepoints baked into font_awesome_icons.c.
// ==============================================================

#define FA_ICON_SINGLE "\xEE\x80\xBC"                  // 0xe03c
#define FA_ICON_VERSUS "\xEE\x80\xBD"                  // 0xe03d
#define FA_ICON_BATTERY_EMPTY "\xEF\x89\x84"           // 0xf244
#define FA_ICON_BATTERY_LOW "\xEE\x82\xB1"             // 0xe0b1
#define FA_ICON_BATTERY_QUARTER "\xEF\x89\x83"         // 0xf243
#define FA_ICON_BATTERY_HALF "\xEF\x89\x82"            // 0xf242
#define FA_ICON_BATTERY_THREE_QUARTERS "\xEF\x89\x81"  // 0xf241
#define FA_ICON_BATTERY_FULL "\xEF\x89\x80"            // 0xf240
#define FA_ICON_BATTERY_CHARGING "\xEF\x8D\xB6"        // 0xf376
#define FA_ICON_BASE_LIFE "\xEE\x80\xB4"               // 0xe034
#define FA_ICON_HIDE_MODE "\xEF\x81\xB0"               // 0xf070
#define FA_ICON_SHOW_MODE "\xEF\x81\xAE"               // 0xf06e
#define FA_ICON_AUTO_MODE "\xEF\x8A\xA8"               // 0xf2a8
#define FA_ICON_BRIGHTNESS "\xEE\x83\x89"              // 0xe0c9
#define FA_ICON_CANCEL "\xEF\x80\x8D"                  // 0xf00d
#define FA_ICON_UNDO "\xEE\x9F\xA7"                    // 0xe7e7
#define FA_ICON_RESET "\xEF\x83\xA2"                   // 0xf0e2
#define FA_ICON_COUNT_UP "\xEF\xA2\x87"                // 0xf887
#define FA_ICON_COUNT_DOWN "\xEF\xA2\x86"              // 0xf886
#define FA_ICON_POWER "\xEF\x80\x91"                   // 0xf011
#define FA_ICON_SLEEP_ZZZ "\xEF\xA2\x80"               // 0xf880
#define FA_ICON_PERCENTAGE "\x25"                      // 0x25

// ==============================================================
// UI text. Centralised so all user-visible captions live in one place
// and translations / wording tweaks don't require hunting through the
// rest of the codebase.
// ==============================================================

namespace UiText {
static constexpr const char* PLAYERS = "PLAYERS";
static constexpr const char* SINGLE = "SINGLE";
static constexpr const char* VERSUS = "VERSUS";
static constexpr const char* COUNT = "COUNT";
static constexpr const char* DOWN = "DOWN";
static constexpr const char* UP = "UP";
static constexpr const char* COUNT_DOWN = "HP";
static constexpr const char* COUNT_UP = "DAMAGE";
static constexpr const char* UNDO = "UNDO";
static constexpr const char* RESET = "RESET";
static constexpr const char* SLEEP = "SLEEP";
static constexpr const char* SHOW_PERCENT = "SHOW %";
static constexpr const char* STATE_ENABLED = "ENABLED";
static constexpr const char* STATE_DISABLED = "DISABLED";
static constexpr const char* BRIGHTNESS = "BRIGHTNESS";
static constexpr const char* BASE_LIFE = "BASE LIFE";
static constexpr const char* BATTERY = "BATTERY";

static constexpr const char* BATTERY_HIDE = "HIDE";
static constexpr const char* BATTERY_AUTO = "AUTO";
static constexpr const char* BATTERY_SHOW = "SHOW";
static constexpr const char* BATTERY_PERCENT_ON = "% ON";

static constexpr const char* BASE_LIFE_FMT_1P = "BASE LIFE %d";
static constexpr const char* BASE_LIFE_FMT_2P = "BASE LIFE %d | %d";
static constexpr const char* BATTERY_FMT = "BATTERY %s";
static constexpr const char* BRIGHTNESS_FMT = "BRIGHTNESS %d%%";

static constexpr const char* BASE_LOST = "BASE LOST";
static constexpr const char* BASE_LOST_P1 = "P1\nBASE LOST";
static constexpr const char* BASE_LOST_P2 = "P2\nBASE LOST";
}  // namespace UiText

// ==============================================================
// Pins (Waveshare ESP32-S3-Touch-LCD-1.28-B board map)
// ==============================================================

constexpr uint8_t PIN_BATTERY_ADC = 1;
constexpr uint8_t PIN_LCD_BACKLIGHT = 2;
constexpr uint8_t PIN_TOUCH_INT = 5;  // also deep-sleep wakeup
constexpr uint8_t PIN_TOUCH_SDA = 6;
constexpr uint8_t PIN_TOUCH_SCL = 7;
constexpr uint8_t PIN_TOUCH_RST = 13;
constexpr uint8_t CST816S_I2C_ADDR = 0x15;  // for raw finger-count reads

// ==============================================================
// Display geometry
// ==============================================================

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 240;
constexpr int CENTER_X = SCREEN_W / 2;
constexpr int CENTER_Y = SCREEN_H / 2;
constexpr int CENTER_TAP_HALF = 32;

// ==============================================================
// Game rules
// ==============================================================

constexpr int STARTING_LIFE = 30;
constexpr int LIFE_MIN = 0;
constexpr int LIFE_MAX = 99;

// ==============================================================
// Life-counter thresholds and animations
//
// All colour/pulse decisions are driven by "distance to defeat":
//   count-down: distance = value         (defeated at 0)
//   count-up:   distance = base - value  (defeated at base)
// ==============================================================

// Colour thresholds (absolute distance values).
//   distance >  LIFE_ZONE_YELLOW_MAX  -> normal (white)
//   distance <= LIFE_ZONE_YELLOW_MAX  -> yellow zone
//   distance <= LIFE_ZONE_RED_MAX     -> red zone
//   distance == 0                     -> defeated
constexpr int LIFE_ZONE_YELLOW_MAX = 10;
constexpr int LIFE_ZONE_RED_MAX = 5;

// Low-HP pulse: ambient opacity throb while in the red zone.
constexpr uint32_t LIFE_PULSE_PERIOD_MS = 1500;
constexpr uint8_t LIFE_PULSE_OPA_MIN = 140;  // ~55% — quite visible drop

// Max/min bump: brief multi-cue animation when a tap is rejected by the
// bounds. Three layered effects fire together so the rejection is
// unmistakable:
//   - horizontal "head shake" wobble (two left-right swings)
//   - brief deep-grey colour flash (NOT red — that would read as damage)
//   - subtle opacity dip
constexpr uint32_t LIFE_BUMP_MS = 360;
constexpr int LIFE_BUMP_SHAKE_AMP = 12;  // px — strong, unambiguous wobble

// Reset celebration: animate from 0 to target value (or target to 0
// for count-up) over this duration when resetBoth() fires.
constexpr uint32_t LIFE_RESET_ANIM_MS = 320;

// ==============================================================
// Defeat overlay
// ==============================================================

constexpr uint32_t DEFEAT_FREEZE_MS = 280;
constexpr uint32_t DEFEAT_PULSE_MS = 380;
constexpr uint8_t DEFEAT_PULSE_COUNT = 3;
constexpr uint32_t DEFEAT_MAIN_MS = DEFEAT_FREEZE_MS + DEFEAT_PULSE_MS * DEFEAT_PULSE_COUNT;

// ==============================================================
// General timing
// ==============================================================

// FLASH_MS is the fade-out duration for the +/- arc. A linear opacity
// ramp reads as a deliberate flash rather than a flicker.
constexpr uint32_t FLASH_MS = 260;
constexpr uint32_t LOOP_DELAY_MS = 5;

// Each CST816S gesture event represents a discrete user action, so we
// accept every event but require at least this much time since the
// previous one fired. 40 ms allows up to 25 taps/second, far faster
// than any human can tap, while preventing accidental double-fire
// from electrical jitter or library quirks.
constexpr uint32_t TOUCH_COOLDOWN_MS = 40;

constexpr uint32_t CENTER_HOLD_MS = 850;         // soft timer before menu opens
constexpr uint32_t RESET_HOLD_MS = 800;          // hold time to confirm a reset
constexpr uint32_t MENU_RELEASE_GRACE_MS = 450;  // fallback when raw I2C touch read fails
constexpr uint32_t MENU_DWELL_REVEAL_MS = 900;
constexpr uint32_t MENU_DWELL_BACK_MS = 900;
constexpr uint32_t MENU_DWELL_COMMIT_MS = 900;
// After dragging a value in a radial sub-view and lifting the finger,
// auto-commit and close the entire menu after this idle period.
// Centre-tap to confirm (return to top ring) still works independently.
constexpr uint32_t MENU_AUTO_COMMIT_IDLE_MS = 600;
constexpr int MENU_INNER_RADIUS = 56;
constexpr int MENU_HIT_INNER_RADIUS = 42;
constexpr int MENU_OUTER_RADIUS = (SCREEN_W / 2) - 2;
constexpr int MENU_RING_THICKNESS = 52;
constexpr uint32_t IMU_POLL_MS = 50;
constexpr uint32_t LVGL_TICK_INTERVAL_MS = 5;

constexpr uint32_t DIM_TIMEOUT_MS = 30UL * 1000UL;       // dim after 30s
constexpr uint32_t SCREEN_PRE_OFF_MS = 85UL * 1000UL;    // pre-off warning at 85s
constexpr uint32_t SCREEN_OFF_MS = 90UL * 1000UL;        // screen off after 90s
constexpr uint32_t DEEP_SLEEP_MS = 3UL * 60UL * 1000UL;  // deep sleep after 3min

// ==============================================================
// Centre-label layout (used by CentreLabel inside the radial menu)
// ==============================================================

constexpr int CENTRE_ICON_Y = -32;
constexpr int CENTRE_TITLE_Y = -8;
constexpr int CENTRE_VALUE_Y = 14;
constexpr int CENTRE_CLOSE_ICON_Y = -24;
constexpr int CENTRE_CLOSE_TITLE_Y = 12;

// ==============================================================
// Backlight levels + percent <-> PWM conversion
// ==============================================================

constexpr uint8_t BACKLIGHT_DIM_LEVEL = 15;
constexpr uint8_t BACKLIGHT_PRE_OFF_LEVEL = 5;  // even dimmer for ~5s before screen-off
constexpr uint8_t BACKLIGHT_MIN_LEVEL = 15;
constexpr uint8_t BACKLIGHT_MAX_LEVEL = 255;
constexpr uint8_t BACKLIGHT_DEFAULT_LEVEL = 75;  // ~25 % of user-adjustable range

// Single source of truth for percent <-> PWM level conversion. Used by
// Backlight, the brightness step selector, and the brightness-cycle action.
constexpr uint8_t backlightPercentToLevel(int pct) {
  return (uint8_t)(BACKLIGHT_MIN_LEVEL + (BACKLIGHT_MAX_LEVEL - BACKLIGHT_MIN_LEVEL) * pct / 100);
}
constexpr int backlightLevelToPercent(uint8_t level) {
  return (level <= BACKLIGHT_MIN_LEVEL)   ? 0
         : (level >= BACKLIGHT_MAX_LEVEL) ? 100
                                          : ((int)level - BACKLIGHT_MIN_LEVEL) * 100 / (BACKLIGHT_MAX_LEVEL - BACKLIGHT_MIN_LEVEL);
}

constexpr int BRIGHTNESS_STEPS[] = { 0, 25, 50, 75, 100 };
constexpr int BRIGHTNESS_STEP_COUNT = sizeof(BRIGHTNESS_STEPS) / sizeof(BRIGHTNESS_STEPS[0]);
constexpr int BRIGHTNESS_STEP_TICK_SIZE = 8;

// Snap an arbitrary percent to the nearest BRIGHTNESS_STEPS value.
inline int snapBrightnessPercent(int pct) {
  pct = constrain(pct, BRIGHTNESS_STEPS[0], BRIGHTNESS_STEPS[BRIGHTNESS_STEP_COUNT - 1]);
  int best = BRIGHTNESS_STEPS[0];
  int bestDist = abs(pct - best);
  for (int i = 1; i < BRIGHTNESS_STEP_COUNT; ++i) {
    const int dist = abs(pct - BRIGHTNESS_STEPS[i]);
    if (dist < bestDist) {
      best = BRIGHTNESS_STEPS[i];
      bestDist = dist;
    }
  }
  return best;
}

// ==============================================================
// CST816S gesture IDs
// ==============================================================

namespace Gesture {
constexpr int SWIPE_UP = 1;
constexpr int SWIPE_DOWN = 2;
constexpr int SWIPE_LEFT = 3;
constexpr int SWIPE_RIGHT = 4;
constexpr int SINGLE_TAP = 5;
// LONG_PRESS (12) is intentionally unused; the soft hold timer in
// TouchRouter is the single source of truth for menu-open detection.
}  // namespace Gesture

// ==============================================================
// IMU (QMI8658C)
// ==============================================================

constexpr uint8_t QMI8658_ADDR = 0x6B;
constexpr float SHAKE_THRESHOLD = 1.6f;
constexpr uint32_t SHAKE_COOLDOWN_MS = 3000;
constexpr uint8_t SHAKE_REVERSALS_REQUIRED = 4;
constexpr uint32_t SHAKE_WINDOW_MS = 1200;

// ==============================================================
// Battery widget
// ==============================================================

constexpr float BATTERY_DIVIDER_RATIO = 3.0f;
constexpr float BATTERY_EMPTY_VOLTS = 3.30f;
constexpr float BATTERY_FULL_VOLTS = 4.20f;
constexpr float BATTERY_PRESENT_MIN_VOLTS = 3.00f;
constexpr float BATTERY_USB_ONLY_MIN_VOLTS = 4.00f;  // USB/no-battery heuristic from observed readings
constexpr uint32_t BATTERY_UPDATE_MS = 120000;
constexpr uint32_t BATTERY_CHARGE_ANIM_MS = 650;
constexpr int BATTERY_AUTO_THRESHOLD = 25;
constexpr int BATTERY_ARC_WIDTH = 4;  // rim width; flash arcs inset by this when battery ring is visible

// ==============================================================
// Colours
// ==============================================================

static const lv_color_t COLOR_BG = lv_color_hex(0x000000);
static const lv_color_t COLOR_FG = lv_color_hex(0xFFFFFF);
static const lv_color_t COLOR_PLUS = lv_color_hex(0x04C000);
static const lv_color_t COLOR_MINUS = lv_color_hex(0xB60000);
static const lv_color_t COLOR_BAT_YELLOW = lv_color_hex(0xE6C200);
static const lv_color_t COLOR_MENU_BLUE = lv_color_hex(0x3A86FF);
static const lv_color_t COLOR_MENU_ORANGE = lv_color_hex(0xFB5607);
static const lv_color_t COLOR_MENU_PINK = lv_color_hex(0xFF006E);
static const lv_color_t COLOR_MENU_YELLOW = lv_color_hex(0xFFD60A);
static const lv_color_t COLOR_MENU_TEAL = lv_color_hex(0x008080);
static const lv_color_t COLOR_MENU_SLEEP = lv_color_hex(0xA02020);
static const lv_color_t COLOR_RING_BG = lv_color_hex(0x2A2A2A);
static const lv_color_t COLOR_BATTERY_HIDE = lv_color_hex(0x606060);
static const lv_color_t COLOR_VALUE_GREY = lv_color_hex(0xAAAAAA);
static const lv_color_t COLOR_DIVIDER = lv_color_hex(0x888888);

// ==============================================================
// Enums shared across modules
// ==============================================================

enum class MenuAction : uint8_t {
  NONE,
  PLAYER_TOGGLE,
  COUNT_DIRECTION,
  BATTERY,
  BRIGHTNESS,
  SLEEP,
  BASE_SELECTOR,
  // Hidden option-actions used by the bound choice view.
  SET_1P,
  SET_2P,
  COUNT_DOWN,
  COUNT_UP,
  SLEEP_OFF,
  BATTERY_CYCLE,
  BRIGHTNESS_CYCLE,
  BASE_SELECTOR_COMMIT
};
constexpr uint8_t MENU_ACTION_COUNT = 6;  // top-level segments

enum class BatteryMode : uint8_t {
  HIDE = 0,
  AUTO = 1,
  SHOW = 2
};

// ==============================================================
// Shared structs
// ==============================================================

// PolarHit lifts the dx/dy/r2/atan2 boilerplate that would otherwise
// appear in every menu view's hit-test into a single helper.
struct PolarHit {
  bool inCentre;  // inside MENU_HIT_INNER_RADIUS
  bool inRing;    // between inner and outer radius
  float deg;      // 0..360 (0 = 3 o'clock, increasing clockwise)
};

inline PolarHit polarFromCenter(int x, int y) {
  const int dx = x - CENTER_X;
  const int dy = y - CENTER_Y;
  const int r2 = dx * dx + dy * dy;
  PolarHit h{};
  h.inCentre = r2 < MENU_HIT_INNER_RADIUS * MENU_HIT_INNER_RADIUS;
  h.inRing = !h.inCentre && r2 <= MENU_OUTER_RADIUS * MENU_OUTER_RADIUS;
  if (!h.inRing) return h;
  float deg = atan2f((float)dy, (float)dx) * 180.0f / PI;
  if (deg < 0) deg += 360.0f;
  h.deg = deg;
  return h;
}

inline float normalizeDeg(float deg) {
  while (deg < 0.0f) deg += 360.0f;
  while (deg >= 360.0f) deg -= 360.0f;
  return deg;
}

inline float absAngleDiff(float a, float b) {
  float d = fabsf(normalizeDeg(a) - normalizeDeg(b));
  return (d > 180.0f) ? (360.0f - d) : d;
}

// Why the device last entered deep sleep. Stashed in RTC memory before
// `esp_deep_sleep_start()` so that on wake the `.ino` can distinguish
// "the user asked to sleep" from "we timed out after idle".
//
// On a fresh power-on the RTC section is zero-initialised, so
// `lastSleepReason == None` naturally means "this is a cold boot".
enum class SleepReason : uint8_t {
  None        = 0,  // never slept (cold-boot default)
  Manual      = 1,  // user picked Sleep from the radial menu
  IdleTimeout = 2,  // backlight idle chain reached DEEP_SLEEP_MS
};

// RTC-persistent state — survives deep sleep, cleared on power-off.
// Defined here so PowerManager and the .ino setup() can both see the layout.
struct PersistentState {
  int life = STARTING_LIFE;
  int life2 = STARTING_LIFE;
  bool countUp = false;
  bool twoPlayer = false;
  bool touchLocked = false;
  int baseLife1 = 30;
  int baseLife2 = 30;
  // Reason for the most recent deep sleep. Used by the boot path to decide
  // whether to replay the startup intro (yes on manual sleep, no on idle).
  SleepReason lastSleepReason = SleepReason::None;
};