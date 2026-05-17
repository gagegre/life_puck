// =============================================================================
// StartupIntro.cpp
//
// Per-phase implementation of the cinematic intro. See StartupIntro.h for the
// high-level story and the phase-boundary timestamps.
//
// Code layout, top to bottom:
//
//   * file-local constants and tiny math helpers
//   * lifecycle: start / skip / construct / destruct / create / finish
//   * `tick()` — phase dispatcher
//   * the five phase methods, in display order:
//       - drawStarfield
//       - drawFighterPass
//       - drawHorizontalSwoosh
//       - drawIconParade
//       - drawLockOnAndReveal
//   * helper methods (hideAllObjects, setHidden, applyIconStyle, timerThunk)
//
// All phases follow the same contract: they look at the elapsed time `t`,
// no-op if `t` is outside their window, and otherwise un-hide and position
// only the objects they need. `tick()` always calls `hideAllObjects()` first,
// so phases never need to "clean up" — they just paint.
// =============================================================================

#include "StartupIntro.h"
#include "IntroIcons.h"

#include <algorithm>

extern "C" {
LV_FONT_DECLARE(font_awesome_intro_fighter);
LV_FONT_DECLARE(font_awesome_intro_characters);
}

namespace {

// ---- palette ---------------------------------------------------------------
const lv_color_t C_BLACK = lv_color_hex(0x000000);
const lv_color_t C_CORE = lv_color_hex(0xE8F6FF);  // bright white, crisp inner lines
const lv_color_t C_GLOW = lv_color_hex(0x69C9FF);  // soft blue halo
const lv_color_t C_DIM = lv_color_hex(0x2C5F82);
const lv_color_t C_LAUNCH_CORE = lv_color_hex(0xFF3344);  // red engine core
const lv_color_t C_LAUNCH_GLOW = lv_color_hex(0xC1121F);  // red engine glow
const lv_color_t C_LAUNCH_DIM = lv_color_hex(0x5A0810);
const lv_color_t C_BOBA = lv_color_hex(0x7BCB78);
const lv_color_t C_LEA = lv_color_hex(0xF4F8FF);
const lv_color_t C_VADER = lv_color_hex(0xFF4C54);
const lv_color_t C_R2 = lv_color_hex(0x5BBEFF);

// ---- math helpers ---------------------------------------------------------

uint8_t clampU8(int value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return static_cast<uint8_t>(value);
}

// Linear interpolation from `a` to `b` over `duration` ms. `elapsed >= duration`
// clamps to the end value; this lets phases lerp freely without bounds-checks.
int16_t lerpI16(int16_t a, int16_t b, uint32_t elapsed, uint32_t duration) {
  if (duration == 0) return b;
  if (elapsed >= duration) return b;
  return static_cast<int16_t>(a + ((int32_t)(b - a) * (int32_t)elapsed) / (int32_t)duration);
}

uint8_t lerpU8(uint8_t a, uint8_t b, uint32_t elapsed, uint32_t duration) {
  if (duration == 0) return b;
  if (elapsed >= duration) return b;
  return clampU8(a + ((int32_t)(b - a) * (int32_t)elapsed) / (int32_t)duration);
}

// Quadratic easings, returned as a 0..255 fraction so they compose with lerpI16.
uint16_t easeInQuad255(uint32_t elapsed, uint32_t duration) {
  if (duration == 0 || elapsed >= duration) return 255;
  const uint32_t p = (elapsed * 255UL) / duration;
  return static_cast<uint16_t>((p * p) / 255UL);
}

uint16_t easeOutQuad255(uint32_t elapsed, uint32_t duration) {
  if (duration == 0 || elapsed >= duration) return 255;
  const uint32_t p = (elapsed * 255UL) / duration;
  return static_cast<uint16_t>(255UL - (((255UL - p) * (255UL - p)) / 255UL));
}

// ---- LVGL object factories -------------------------------------------------

// Plain coloured rectangle, no border/outline/shadow. Starts hidden because
// every phase opt-ins by clearing the hidden flag.
lv_obj_t* makeRect(lv_obj_t* parent, lv_color_t color, uint8_t opa, int16_t w, int16_t h) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, color, 0);
  lv_obj_set_style_bg_opa(obj, opa, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  return obj;
}

// Label rendered as a single FontAwesome glyph, with a soft halo via LVGL's
// shadow style. We do NOT use this factory for the X-Wing fighter (label
// shadows can read as a rectangular box around the glyph at low alpha).
lv_obj_t* makeIcon(lv_obj_t* parent, const char* glyph, const lv_font_t* font, uint8_t opa) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_remove_style_all(label);
  lv_obj_set_style_bg_opa(label, LV_OPA_0, 0);
  lv_obj_set_style_border_width(label, 0, 0);
  lv_obj_set_style_outline_width(label, 0, 0);
  lv_obj_set_style_pad_all(label, 0, 0);
  lv_obj_set_style_radius(label, 0, 0);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, C_CORE, 0);
  lv_obj_set_style_text_opa(label, opa, 0);
  lv_obj_set_style_shadow_color(label, C_GLOW, 0);
  lv_obj_set_style_shadow_width(label, 18, 0);
  lv_obj_set_style_shadow_opa(label, LV_OPA_40, 0);
  lv_obj_set_style_shadow_spread(label, 1, 0);
  lv_label_set_text(label, glyph);
  lv_obj_center(label);
  return label;
}

// Each parade character has a signature accent colour used by both the icon
// itself and the scan line that wipes it in.
lv_color_t iconColor(uint8_t index) {
  switch (index) {
    case 0:
      return C_BOBA;
    case 1:
      return C_LEA;
    case 2:
      return C_VADER;
    default:
      return C_R2;
  }
}

}  // namespace

// =============================================================================
// Lifecycle
// =============================================================================

bool StartupIntro::shouldShowFor(BootReason reason) {
#if ENABLE_STARTUP_INTRO
  // Cold boot and manual sleep both deserve the full intro (the user is
  // either powering on or has explicitly asked for the device to "restart").
  // Idle-timeout wakes skip the intro so a tap-to-wake feels instant.
  switch (reason) {
    case BootReason::ColdBoot:
      return true;
    case BootReason::WakeFromManualSleep:
      return true;
    case BootReason::WakeFromIdleSleep:
      return false;
  }
  return false;
#else
  (void)reason;
  return false;
#endif
}

StartupIntro* StartupIntro::start(lv_obj_t* parent,
                                  FinishedCallback onFinished,
                                  void* userData,
                                  RevealCallback onReveal) {
#if ENABLE_STARTUP_INTRO
  StartupIntro* intro = new StartupIntro(parent, onFinished, userData, onReveal);
  intro->create();
  return intro;
#else
  // Intro compiled out: still run the caller's "intro done" chain so the
  // boot flow doesn't stall on a callback that never fires.
  if (onFinished) onFinished(userData);
  return nullptr;
#endif
}

void StartupIntro::skip(StartupIntro*& intro) {
  if (!intro) return;
  intro->finish();
  intro = nullptr;
}

StartupIntro::StartupIntro(lv_obj_t* parent, FinishedCallback onFinished, void* userData, RevealCallback onReveal)
    : _parent(parent ? parent : lv_scr_act())
    , _onFinished(onFinished)
    , _onReveal(onReveal)
    , _userData(userData) {}

StartupIntro::~StartupIntro() {
  if (_timer) {
    lv_timer_del(_timer);
    _timer = nullptr;
  }
  if (_root) {
    lv_obj_del(_root);
    _root = nullptr;
  }
}

void StartupIntro::create() {
  // Full-screen opaque root over the (already-built but invisible) game UI.
  // CLICKABLE so taps on the intro do not propagate to the life counter
  // underneath while it plays.
  _root = lv_obj_create(_parent);
  lv_obj_remove_style_all(_root);
  lv_obj_set_size(_root, ScreenW, ScreenH);
  lv_obj_center(_root);
  lv_obj_set_style_bg_color(_root, C_BLACK, 0);
  lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);
  lv_obj_add_flag(_root, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_move_foreground(_root);

  // Hand-placed star positions + per-star fade-in delay + base opacity.
  // The pattern is chosen by eye to feel scattered rather than gridded.
  static constexpr Star starSeed[16] = {
      {nullptr, 44, 32, 20, 70, 1},
      {nullptr, 118, 20, 70, 85, 1},
      {nullptr, 184, 38, 120, 55, 1},
      {nullptr, 72, 64, 170, 95, 1},
      {nullptr, 157, 72, 220, 65, 1},
      {nullptr, 211, 86, 270, 80, 1},
      {nullptr, 28, 104, 320, 55, 1},
      {nullptr, 101, 102, 360, 105, 2},
      {nullptr, 197, 124, 410, 60, 1},
      {nullptr, 52, 146, 450, 85, 1},
      {nullptr, 135, 151, 490, 65, 1},
      {nullptr, 219, 165, 530, 50, 1},
      {nullptr, 83, 188, 570, 70, 1},
      {nullptr, 165, 200, 610, 95, 1},
      {nullptr, 35, 214, 340, 45, 1},
      {nullptr, 205, 215, 470, 75, 1},
  };

  for (uint8_t i = 0; i < 16; ++i) {
    _stars[i] = starSeed[i];
    _stars[i].obj = makeRect(_root, C_CORE, 0, _stars[i].size, _stars[i].size);
    lv_obj_set_pos(_stars[i].obj, _stars[i].x, _stars[i].y);
  }

  // Pool of reusable rectangles. Phases re-purpose these across the timeline:
  //   _verticalGlow/_verticalCore: red engine trail (left) -> center pulse glow at the end
  //   _exhaustGlow/_exhaustCore  : repurposed as lock-on vertical ticks at the end
  //   _swoosh[0..1]              : red engine trail (right)
  //   _swoosh[2..8]              : engine cone bands during the fighter pass
  //   _hGlow/_hCore              : horizontal "swoosh" line -> lock-on vertical ticks
  //   _scanline                  : parade scan column -> centre pulse at the end
  //   _fragments[0..3]           : parade scan accent dots -> lock-on horizontal ticks
  _verticalGlow = makeRect(_root, C_LAUNCH_GLOW, 0, 5, 1);
  _verticalCore = makeRect(_root, C_LAUNCH_CORE, 0, 1, 1);
  _exhaustGlow = makeRect(_root, C_LAUNCH_GLOW, 0, 18, 1);
  _exhaustCore = makeRect(_root, C_LAUNCH_CORE, 0, 5, 1);
  for (uint8_t i = 0; i < 4; ++i) {
    _exhaustSpark[i] = makeRect(_root, (i % 2 == 0) ? C_LAUNCH_CORE : C_LAUNCH_GLOW, 0, 2 + i, 1);
  }
  for (uint8_t i = 0; i < 9; ++i) {
    _swoosh[i] = makeRect(_root, (i == 4) ? C_LAUNCH_CORE : C_LAUNCH_GLOW, 0, 10, 4);
  }
  _hGlow = makeRect(_root, C_LAUNCH_GLOW, 0, 1, 7);
  _hCore = makeRect(_root, C_LAUNCH_CORE, 0, 1, 1);
  _scanline = makeRect(_root, C_CORE, 0, 2, 116);

  for (uint8_t i = 0; i < 4; ++i) {
    _fragments[i] = makeRect(_root, C_CORE, 0, 12 + i * 4, 1);
  }

  // X-Wing label. No shadow/backplate: at low alpha LVGL's label shadow
  // can read as a faint rectangle around the glyph, which breaks the
  // "ship moving through space" illusion.
  _fighter = makeIcon(_root, FA_ICON_INTRO_X_WING, &font_awesome_intro_fighter, 0);
  lv_obj_set_style_bg_opa(_fighter, LV_OPA_0, 0);
  lv_obj_set_style_border_width(_fighter, 0, 0);
  lv_obj_set_style_outline_width(_fighter, 0, 0);
  lv_obj_set_style_shadow_width(_fighter, 0, 0);
  lv_obj_set_style_shadow_opa(_fighter, LV_OPA_0, 0);
  lv_obj_set_style_text_color(_fighter, C_CORE, 0);

  const char* iconGlyphs[4] = {
      FA_ICON_INTRO_BOBA,
      FA_ICON_INTRO_LEA,
      FA_ICON_INTRO_VADER,
      FA_ICON_INTRO_R2_D2,
  };

  // Pre-build all four parade icons so transitions just toggle opacity.
  // Font is generated at 80 px so no transform scaling is needed to keep
  // the glyph crisp.
  for (uint8_t i = 0; i < 4; ++i) {
    _icons[i] = makeIcon(_root, iconGlyphs[i], &font_awesome_intro_characters, 0);
    lv_obj_set_style_transform_scale_x(_icons[i], 256, 0);
    lv_obj_set_style_transform_scale_y(_icons[i], 256, 0);
    lv_obj_set_style_shadow_width(_icons[i], 0, 0);
    lv_obj_set_style_shadow_spread(_icons[i], 0, 0);
    lv_obj_set_style_shadow_opa(_icons[i], LV_OPA_0, 0);
    lv_obj_center(_icons[i]);
    lv_obj_add_flag(_icons[i], LV_OBJ_FLAG_HIDDEN);
  }

  // Drive the animation off an LVGL timer at ~62.5 fps. We also call tick()
  // once now so the first paint is correct rather than blank-for-16-ms.
  // ---- Hold-to-skip arc --------------------------------------------------
  // Created last so it sits on top of every animation object. Not part of
  // the animation pool — hideAllObjects() never touches it; drawSkipArc()
  // controls its visibility independently each tick.
  //
  // Indicator:  3 px crisp white ring, fills clockwise from 12 o'clock.
  // Background: 1 px faint dim track shows the full circle as a guide.
  // No knob dot. Not clickable (_root already consumes all touches).
  {
    lv_obj_t* arc = lv_arc_create(_root);
    lv_obj_set_size(arc, ScreenW - 14, ScreenH - 14);
    lv_obj_center(arc);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_bg_angles(arc, 0, 360);  // full-circle background track
    lv_arc_set_angles(arc, 0, 0);       // indicator starts empty
    lv_arc_set_rotation(arc, 270);      // 0° at 12 o'clock → fills clockwise
    lv_obj_set_style_arc_color(arc, C_CORE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_80, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, C_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 1, LV_PART_MAIN);
    lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    _skipArc = arc;
  }

  _startedAt = lv_tick_get();
  _timer = lv_timer_create(timerThunk, 16, this);
  tick();
}

void StartupIntro::finish() {
  if (_finished) return;
  _finished = true;

  // Snapshot callback + userData; the lv_obj_del below tears down everything,
  // and we delete ourselves at the end, so we must not touch members after that.
  FinishedCallback cb = _onFinished;
  void* userData = _userData;

  if (_timer) {
    lv_timer_del(_timer);
    _timer = nullptr;
  }

  if (_root) {
    lv_obj_del(_root);
    _root = nullptr;
  }

  if (cb) cb(userData);
  delete this;
}

// =============================================================================
// Tick — phase dispatcher
// =============================================================================

void StartupIntro::tick() {
  if (_finished) return;

  const uint32_t t = lv_tick_elaps(_startedAt);

  // The final crosshair/reveal phase is performance-sensitive because the
  // life counter may already be animating underneath it. Do not hide/rebuild
  // every intro object each frame during this phase.
  if (t >= TBlackBeatEnd) {
    if (!_revealPrepared) {
      _revealPrepared = true;

      if (_onReveal) _onReveal(_userData);

      // The callback builds the game UI underneath us.
      // Keep intro above it.
      if (_root) lv_obj_move_foreground(_root);

      // Important: do NOT make root transparent here.
      // Keep it black until drawLockOnAndReveal() fades it out.
      if (_root) lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);
    }

    drawLockOnAndReveal(t);
  } else {
    hideAllObjects();

    drawStarfield(t);
    drawFighterPass(t);
    drawHorizontalSwoosh(t);
    drawIconParade(t);
    drawLockOnAndReveal(t);
  }

  // Normal end: animation timeline exhausted.
  if (t >= TTotal) {
    finish();
    return;
  }

  // Hold-to-skip: drawn on top of every phase, independent of t.
  // Must be the very last call — if TSkipHold ms have elapsed it will
  // call finish() → delete this, so nothing may run after it.
  drawSkipArc();
}

// =============================================================================
// Phase 1 — Starfield (0 .. TLineEnd)
//
// Sixteen pinprick stars fade in on their own staggered delays during the
// first ~0.7 s, then twinkle gently with a quantised offset so they don't
// all blink in sync. After the X-Wing pass and the horizontal swoosh have
// finished (TSweepEnd .. TLineEnd), they fade back to zero so the screen
// is cleanly black before the icon parade begins.
// =============================================================================

void StartupIntro::drawStarfield(uint32_t t) {
  for (uint8_t i = 0; i < 16; ++i) {
    Star& s = _stars[i];
    uint8_t opa = 0;
    if (t >= s.delay && t <= TLineEnd) {
      const uint32_t local = t - s.delay;
      // 180 ms fade-in to the star's base opacity.
      const uint8_t appear = lerpU8(0, s.baseOpa, std::min<uint32_t>(local, 180), 180);
      // Cheap twinkle: index-offset modulo of t gives each star its own phase.
      const int8_t twinkle = static_cast<int8_t>(((t / 115 + i) % 4) * 7);
      opa = clampU8(appear - twinkle);
      // After the X-Wing has passed, fade the stars away to clear the stage
      // for the parade.
      if (t > TSweepEnd) opa = lerpU8(opa, 0, t - TSweepEnd, TLineEnd - TSweepEnd);
    }
    lv_obj_set_style_bg_opa(s.obj, opa, 0);
    setHidden(s.obj, opa == 0);
  }
}

// =============================================================================
// Phase 2 — Fighter pass + engine cone (TStarEnd .. TSweepEnd)
//
// The X-Wing rises into view from below the screen, climbs slowly until it
// reaches ~mid-height (the future home of the horizontal "swoosh" line),
// then accelerates upward and exits the top. Two red engine-trail lines
// follow it; at the boost point those trails fade and a soft seven-band
// engine cone reveals itself layer-by-layer above the line. The cone is
// what the horizontal line will emerge from in the next phase.
// =============================================================================

void StartupIntro::drawFighterPass(uint32_t t) {
  if (t < TStarEnd || t >= TSweepEnd) return;

  const uint32_t local = t - TStarEnd;
  const uint32_t dur = TSweepEnd - TStarEnd;

  // Sub-phase timing within this window.
  static constexpr uint32_t TVisibleStart = 180;  // ship starts emerging from below
  static constexpr uint32_t TBoostStart = 1180;   // ship reaches boost point, accelerates
  static constexpr int16_t ShipStartY = 258;      // fully off-screen below the 240 px panel
  static constexpr int16_t ShipBoostY = 98;       // boost begins near screen centre so it
                                                  //   aligns with the horizontal-line phase
  static constexpr int16_t ShipEndY = -70;        // off the top

  int16_t y = ShipStartY;
  uint8_t shipOpa = 0;

  if (local < TVisibleStart) {
    // Hidden below the screen. Nothing to draw yet.
    y = ShipStartY;
    shipOpa = 0;
  } else if (local < TBoostStart) {
    // Slow climb from below to the boost point, eased-out so it decelerates
    // visually as it approaches the centre.
    const uint32_t slowLocal = local - TVisibleStart;
    const uint32_t slowDur = TBoostStart - TVisibleStart;
    const uint16_t eased = easeOutQuad255(slowLocal, slowDur);
    y = lerpI16(ShipStartY, ShipBoostY, eased, 255);
    // Fade-in as soon as the ship crosses the bottom of the panel.
    shipOpa = (y > 220) ? lerpU8(0, 235, ShipStartY - y, ShipStartY - 220) : 235;
  } else {
    // Boost: ease-in from the centre and off the top of the screen, fading
    // out in the final quarter so the ship doesn't pop when it leaves.
    const uint32_t boostLocal = local - TBoostStart;
    const uint32_t boostDur = dur - TBoostStart;
    const uint16_t eased = easeInQuad255(boostLocal, boostDur);
    y = lerpI16(ShipBoostY, ShipEndY, eased, 255);
    shipOpa = (boostLocal < boostDur * 3 / 4) ? 245 : lerpU8(245, 70, boostLocal - boostDur * 3 / 4, boostDur / 4);
  }

  setHidden(_fighter, false);
  lv_obj_align(_fighter, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_style_text_opa(_fighter, shipOpa, 0);
  lv_obj_set_style_text_color(_fighter, C_CORE, 0);
  lv_obj_set_style_shadow_opa(_fighter, LV_OPA_0, 0);

  // Draw the engine trails (and later the cone) only while the ship itself
  // is on-screen. There is intentionally no centre plume during this phase;
  // the centre region is reserved for the horizontal line to emerge into.
  if (shipOpa > 0 && y < 236) {
    const int16_t trailTop = std::max<int16_t>(0, y + 31);
    const int16_t trailHeight = std::max<int16_t>(1, ScreenH - trailTop);
    // Trail brightness ramps up before boost and decays after.
    const uint8_t trailOpaRaw = (local < TBoostStart)
                                    ? lerpU8(120, 230, local - TVisibleStart, TBoostStart - TVisibleStart)
                                    : lerpU8(245, 100, local - TBoostStart, dur - TBoostStart);

    const int16_t lineOffset = 13;
    const int16_t glowW = 4;
    const int16_t coreW = 2;

    // After boost the engine trail rapidly fades to zero (170 ms) so the
    // cone can take the stage cleanly.
    uint8_t engineTrailOpa = trailOpaRaw;
    if (local >= TBoostStart) {
      const uint32_t burstLocal = local - TBoostStart;
      const uint32_t fadeDur = 170;
      engineTrailOpa = (burstLocal < fadeDur) ? lerpU8(trailOpaRaw, 0, burstLocal, fadeDur) : 0;
    }

    if (engineTrailOpa > 0) {
      // Left red line: soft glow + crisp core.
      setHidden(_verticalGlow, false);
      setHidden(_verticalCore, false);
      lv_obj_set_style_bg_color(_verticalGlow, C_LAUNCH_GLOW, 0);
      lv_obj_set_style_bg_color(_verticalCore, C_LAUNCH_CORE, 0);
      lv_obj_set_pos(_verticalGlow, CenterX - lineOffset - glowW / 2, trailTop);
      lv_obj_set_size(_verticalGlow, glowW, trailHeight);
      lv_obj_set_style_bg_opa(_verticalGlow, engineTrailOpa / 2, 0);
      lv_obj_set_pos(_verticalCore, CenterX - lineOffset - coreW / 2, trailTop);
      lv_obj_set_size(_verticalCore, coreW, trailHeight);
      lv_obj_set_style_bg_opa(_verticalCore, engineTrailOpa, 0);

      // Right red line: same construction, mirrored.
      setHidden(_swoosh[0], false);
      setHidden(_swoosh[1], false);
      lv_obj_set_style_bg_color(_swoosh[0], C_LAUNCH_GLOW, 0);
      lv_obj_set_style_bg_color(_swoosh[1], C_LAUNCH_CORE, 0);
      lv_obj_set_pos(_swoosh[0], CenterX + lineOffset - glowW / 2, trailTop);
      lv_obj_set_size(_swoosh[0], glowW, trailHeight);
      lv_obj_set_style_bg_opa(_swoosh[0], engineTrailOpa / 2, 0);
      lv_obj_set_pos(_swoosh[1], CenterX + lineOffset - coreW / 2, trailTop);
      lv_obj_set_size(_swoosh[1], coreW, trailHeight);
      lv_obj_set_style_bg_opa(_swoosh[1], engineTrailOpa, 0);
    }

    // At/after the boost point: reveal the engine cone above the future
    // horizontal line. Layers come in one-by-one (every 42 ms) starting
    // from the widest band and building up, so the cone feels "drawn" by
    // the X-Wing's exhaust rather than appearing all at once.
    if (local >= TBoostStart) {
      const uint32_t burstLocal = local - TBoostStart;
      const uint32_t burstDur = std::max<uint32_t>(1, dur - TBoostStart);

      // Widths and opacity falloffs, base band first (widest, brightest).
      const uint8_t coneBaseOpa = lerpU8(150, 120, std::min<uint32_t>(burstLocal, burstDur), burstDur);
      static const int16_t coneHalfWidthsBaseFirst[7] = {60, 48, 38, 29, 21, 14, 8};
      static const uint8_t coneOpaFalloffBaseFirst[7] = {0, 12, 22, 34, 48, 62, 80};
      const uint8_t revealBands = std::min<uint8_t>(7, static_cast<uint8_t>(1 + (burstLocal / 42)));
      // Bands start tightly stacked and spread vertically as the cone resolves.
      const int16_t coneSpacing = lerpI16(5, 10, std::min<uint32_t>(burstLocal, burstDur), burstDur);
      const int16_t coneRise = lerpI16(0, 8, std::min<uint32_t>(burstLocal, burstDur), burstDur);

      for (uint8_t layer = 0; layer < revealBands; ++layer) {
        const uint8_t i = 8 - layer;  // use _swoosh[8] as base, then 7, 6, ... upward
        const int16_t coneHalf = coneHalfWidthsBaseFirst[layer];
        const uint8_t coneOpa = clampU8((int)coneBaseOpa - coneOpaFalloffBaseFirst[layer]);
        if (coneOpa == 0) continue;

        const int16_t coneY = CenterY - layer * coneSpacing - coneRise;
        if (coneY < -4 || coneY > ScreenH) continue;

        setHidden(_swoosh[i], false);
        lv_obj_set_style_bg_color(_swoosh[i], (layer <= 1) ? C_LAUNCH_CORE : C_LAUNCH_GLOW, 0);
        lv_obj_set_pos(_swoosh[i], CenterX - coneHalf, coneY);
        lv_obj_set_size(_swoosh[i], coneHalf * 2, (layer == 0) ? 3 : 2);
        lv_obj_set_style_bg_opa(_swoosh[i], coneOpa, 0);
      }
    }
  }
}

// =============================================================================
// Phase 3 — Horizontal swoosh + cone dissolve (TSweepEnd .. TLineEnd)
//
// A bright horizontal line bursts outward from the cone base, expanding
// from ~120 px wide to full screen width and then fading. The cone bands
// drift downward and fade out simultaneously, so the line looks like it
// "shed" them. After this phase the screen is essentially black again,
// ready for the parade.
// =============================================================================

void StartupIntro::drawHorizontalSwoosh(uint32_t t) {
  if (t < TSweepEnd || t >= TLineEnd) return;

  const uint32_t local = t - TSweepEnd;
  const uint32_t dur = TLineEnd - TSweepEnd;

  // Two-stage envelope: line pushes outward and brightens for the first
  // ~2/3, then fades for the final ~1/3.
  const int16_t half = lerpI16(60, 120, local, dur * 2 / 3);
  const uint8_t opa =
      (local < dur * 2 / 3) ? lerpU8(175, 235, local, dur * 2 / 3) : lerpU8(235, 0, local - dur * 2 / 3, dur / 3);

  setHidden(_hGlow, false);
  setHidden(_hCore, false);
  lv_obj_set_style_bg_color(_hGlow, C_LAUNCH_GLOW, 0);
  lv_obj_set_style_bg_color(_hCore, C_LAUNCH_CORE, 0);
  // Soft glow first, crisp 1-px core sitting on top: gives the line depth.
  lv_obj_set_pos(_hGlow, CenterX - half, CenterY - 4);
  lv_obj_set_size(_hGlow, std::max<int16_t>(1, half * 2), 11);
  lv_obj_set_style_bg_opa(_hGlow, opa / 2, 0);
  lv_obj_set_pos(_hCore, CenterX - half, CenterY);
  lv_obj_set_size(_hCore, std::max<int16_t>(1, half * 2), 1);
  lv_obj_set_style_bg_opa(_hCore, opa, 0);

  // Cone dissolve: during the line's brightening half, the seven cone bands
  // from the previous phase drift downward and fade. We reuse `_swoosh[2..8]`
  // here (the same bands that built the cone) so it's the *same* visual
  // object dissolving away.
  if (local < dur * 2 / 3) {
    const uint32_t dissolveLocal = local;
    const uint32_t dissolveDur = dur * 2 / 3;
    static const int16_t coneHalfWidthsBaseFirst[7] = {60, 48, 38, 29, 21, 14, 8};
    static const uint8_t coneStartOpaBaseFirst[7] = {105, 96, 88, 78, 66, 54, 40};
    const int16_t coneSpacing = 10;

    for (uint8_t layer = 0; layer < 7; ++layer) {
      const uint8_t i = 8 - layer;
      const int16_t remHalf = coneHalfWidthsBaseFirst[layer];
      const int16_t baseY = CenterY - layer * coneSpacing;
      // Each upper band drifts a little farther — the higher layers fall faster.
      const int16_t driftY = baseY + lerpI16(0, 28 + layer * 4, dissolveLocal, dissolveDur);
      const uint8_t remOpa = lerpU8(coneStartOpaBaseFirst[layer], 0, dissolveLocal, dissolveDur);
      if (remOpa == 0 || driftY > ScreenH) continue;
      setHidden(_swoosh[i], false);
      lv_obj_set_style_bg_color(_swoosh[i], (layer <= 1) ? C_LAUNCH_CORE : C_LAUNCH_GLOW, 0);
      lv_obj_set_pos(_swoosh[i], CenterX - remHalf, driftY);
      lv_obj_set_size(_swoosh[i], remHalf * 2, (layer == 0) ? 3 : 2);
      lv_obj_set_style_bg_opa(_swoosh[i], remOpa, 0);
    }
  }
}

// =============================================================================
// Phase 4 — Icon parade (TIconStart .. TFinalHoldEnd)
//
// Four character glyphs cycle through the centre, each wiped in by a coloured
// vertical scan line moving across the screen:
//
//   * Boba   (green, left-to-right)   — no previous icon to fade out
//   * Lea    (white, right-to-left)
//   * Vader  (red,   left-to-right)
//   * R2-D2  (blue,  right-to-left, with a final brightening "lock" glow)
//
// `t >= TFinalHoldEnd` is the handoff to drawLockOnAndReveal(), which finishes
// the R2 fade and brings up the lock-on brackets.
// =============================================================================

void StartupIntro::drawIconParade(uint32_t t) {
  if (t < TIconStart || t >= TFinalHoldEnd) return;

  // ---- closures shared by all four sub-phases --------------------------

  // Reveal an icon at a given opacity, with a shadow brightness boost during
  // the scan-in for a satisfying "appearing" pulse.
  auto showIcon = [&](uint8_t index, uint8_t opa) {
    setHidden(_icons[index], false);
    applyIconStyle(_icons[index], iconColor(index), opa);
  };

  // Draw the scan column plus a tiny "data-slice" distortion across the
  // current glyph. The slices are deliberately sparse: enough to read as a
  // Star-Wars/datapad scan, not enough to turn into noise on the small LCD.
  auto drawScan = [&](uint32_t start, uint32_t end, bool leftToRight, lv_color_t scanColor) {
    const uint32_t local = t - start;
    const uint32_t dur = end - start;
    const int16_t dir = leftToRight ? 1 : -1;
    const int16_t x = leftToRight ? lerpI16(48, 190, local, dur) : lerpI16(190, 48, local, dur);
    const uint8_t pulseOpa = ((local / 48) % 2 == 0) ? LV_OPA_70 : LV_OPA_40;

    setHidden(_scanline, false);
    lv_obj_set_pos(_scanline, x, 62);
    lv_obj_set_size(_scanline, 2, 116);
    lv_obj_set_style_bg_color(_scanline, scanColor, 0);
    lv_obj_set_style_bg_opa(_scanline, LV_OPA_100, 0);

    // Two short horizontal cuts cross the glyph near the scan column. They
    // slide with the scan direction, giving the transition a digital wipe.
    setHidden(_fragments[0], false);
    lv_obj_set_pos(_fragments[0], x - (leftToRight ? 34 : 6), CenterY - 18);
    lv_obj_set_size(_fragments[0], 40, 2);
    lv_obj_set_style_bg_color(_fragments[0], scanColor, 0);
    lv_obj_set_style_bg_opa(_fragments[0], pulseOpa, 0);

    setHidden(_fragments[1], false);
    lv_obj_set_pos(_fragments[1], x - (leftToRight ? 14 : 28), CenterY + 16);
    lv_obj_set_size(_fragments[1], 30, 2);
    lv_obj_set_style_bg_color(_fragments[1], scanColor, 0);
    lv_obj_set_style_bg_opa(_fragments[1], LV_OPA_50, 0);

    // Two small trailing specs make the scan feel like it has momentum without
    // costing any extra LVGL objects.
    for (uint8_t i = 2; i < 4; ++i) {
      const int16_t y = (i == 2) ? CenterY - 36 : CenterY + 38;
      const int16_t offset = (i == 2) ? 18 : 28;
      setHidden(_fragments[i], false);
      lv_obj_set_pos(_fragments[i], x - dir * offset, y);
      lv_obj_set_size(_fragments[i], 12, 2);
      lv_obj_set_style_bg_color(_fragments[i], scanColor, 0);
      lv_obj_set_style_bg_opa(_fragments[i], LV_OPA_40, 0);
    }
  };

  // Cross-fade between two icons while the scan column sweeps past.
  auto transition = [&](uint8_t from, uint8_t to, uint32_t start, uint32_t end, bool leftToRight) {
    const uint32_t local = t - start;
    const uint32_t dur = end - start;
    const uint8_t fromOpa = lerpU8(235, 0, local, dur);
    const uint8_t toOpa = lerpU8(0, 235, local, dur);
    showIcon(from, fromOpa);
    showIcon(to, toOpa);
    drawScan(start, end, leftToRight, iconColor(to));
  };

  // Initial introduction (Boba): same scan but no outgoing icon.
  auto introTransitionTo = [&](uint8_t to, uint32_t start, uint32_t end, bool leftToRight) {
    const uint32_t local = t - start;
    const uint32_t dur = end - start;
    const uint8_t toOpa = lerpU8(0, 235, local, dur);
    showIcon(to, toOpa);
    drawScan(start, end, leftToRight, iconColor(to));
  };

  // ---- four sub-phases, time-sliced ------------------------------------

  if (t < TFirstHoldEnd) {
    // 1. Boba scan-in (LTR green), then a brief hold.
    static constexpr uint32_t TBobaScanEnd = TIconStart + TIconScanMs;
    if (t < TBobaScanEnd) {
      introTransitionTo(0, TIconStart, TBobaScanEnd, true);
    } else {
      showIcon(0, 225);
    }
  } else if (t < TTrans0End) {
    // 2. Boba -> Lea (RTL white).
    transition(0, 1, TFirstHoldEnd, TTrans0End, false);
  } else if (t < THold1End) {
    // 2a. Lea hold.
    showIcon(1, 225);
  } else if (t < TTrans1End) {
    // 3. Lea -> Vader (LTR red).
    transition(1, 2, THold1End, TTrans1End, true);
  } else if (t < THold2End) {
    // 3a. Vader hold.
    showIcon(2, 225);
  } else if (t < TTrans2End) {
    // 4. Vader -> R2-D2 (RTL blue).
    transition(2, 3, THold2End, TTrans2End, false);
  } else {
    // 4a. R2 hold, with a 260 ms brightening pulse near the end so the
    //     final glyph "locks in" before the lock-on brackets appear.
    const uint8_t glow = (t > TFinalHoldEnd - 260) ? lerpU8(225, 255, t - (TFinalHoldEnd - 260), 260) : 225;
    showIcon(3, glow);
  }
}

// =============================================================================
// Phase 5 — Lock-on brackets + reveal (TFinalHoldEnd .. TTotal)
//
// Final beat. R2 fades to black; the overlay holds black for a few frames;
// then the black overlay starts fading away, a soft centre glow + crisp
// centre scanline pulse, and four corner "lock-on" tick marks pulse in and
// out around where the life counter will be. By TTotal the overlay is fully
// transparent and finish() tears the intro down, handing control to the
// real game UI underneath.
// =============================================================================

void StartupIntro::drawLockOnAndReveal(uint32_t t) {
  if (t < TFinalHoldEnd || t >= TTotal) return;

  // First frame of the final reveal phase: hide the old parade/ship objects
  // once. After this, only update the crosshair objects.
  if (t >= TLockStart && !_lockRevealPrepared) {
    _lockRevealPrepared = true;

    setHidden(_fighter, true);

    for (uint8_t i = 0; i < 16; ++i) {
      setHidden(_stars[i].obj, true);
    }

    for (uint8_t i = 0; i < 4; ++i) {
      setHidden(_icons[i], true);
      setHidden(_exhaustSpark[i], true);
    }

    for (uint8_t i = 0; i < 9; ++i) {
      setHidden(_swoosh[i], true);
    }

    setHidden(_hGlow, true);
    setHidden(_hCore, true);
    setHidden(_verticalGlow, true);
    setHidden(_verticalCore, true);
    setHidden(_exhaustGlow, true);
    setHidden(_exhaustCore, true);
    setHidden(_scanline, true);

    // Keep the root black here. drawLockOnAndReveal() will fade it out
    // gradually during the lock phase, avoiding a sudden flash to the UI.
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);
  }

  // Tail end of R2's fade-out, finishing in the small black-beat window.
  if (t < TBlackBeatEnd) {
    const uint8_t fade = lerpU8(255, 0, t - TFinalHoldEnd, TBlackBeatEnd - TFinalHoldEnd);
    setHidden(_icons[3], false);
    applyIconStyle(_icons[3], iconColor(3), fade);
  }

  // Keep the root fully black during the tiny black beat. Once the lock
  // phase starts, fade the black away over the same duration as the reset
  // count-up underneath. This avoids an instant transparent-frame flash.
  if (t < TLockStart) {
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);
    return;
  }

  const uint32_t local = t - TLockStart;
  const uint32_t fadeLocal = local > TRevealFadeDelay ? local - TRevealFadeDelay : 0;
  const uint32_t fadeDur = TRevealEnd - TLockStart - TRevealFadeDelay;
  const uint8_t bgOpa = (t < TRevealEnd) ? lerpU8(LV_OPA_COVER, 0, fadeLocal, fadeDur) : 0;
  lv_obj_set_style_bg_opa(_root, bgOpa, 0);

  // ---- centre scanline pulse ---------------------------------------------
  // A horizontal sliver that starts narrow, widens, briefly "acquires" the
  // target, then fades. This makes the reveal read as calibration instead of
  // just a transparent overlay fading away.
  static constexpr uint32_t TAcquirePulseMs = 140;
  const uint8_t acquirePulse = (local < TAcquirePulseMs) ? lerpU8(0, 70, local, TAcquirePulseMs)
                                                         : lerpU8(70, 0, local - TAcquirePulseMs, TAcquirePulseMs);
  const uint8_t baseScanOpa = (t < TLockPulseEnd) ? lerpU8(0, 230, local, TLockPulseEnd - TLockStart)
                                                  : lerpU8(230, 60, t - TLockPulseEnd, TRevealEnd - TLockPulseEnd);
  const uint8_t scanOpa = clampU8(baseScanOpa + acquirePulse);
  const int16_t scanHalf = (t < TLockPulseEnd) ? lerpI16(14, 92, local, TLockPulseEnd - TLockStart)
                                               : lerpI16(92, 112, t - TLockPulseEnd, TRevealEnd - TLockPulseEnd);
  setHidden(_scanline, false);
  lv_obj_set_pos(_scanline, CenterX - scanHalf, CenterY - 2);
  lv_obj_set_size(_scanline, scanHalf * 2, 3);
  lv_obj_set_style_bg_color(_scanline, C_CORE, 0);
  lv_obj_set_style_bg_opa(_scanline, scanOpa, 0);

  // ---- soft centre glow --------------------------------------------------
  // Big 28 px blue halo + 8 px bright core, pulsing in and out together
  // with the scanline. This is what the eye reads as the "lock target".
  const uint8_t glowOpa = (t < TLockPulseEnd) ? lerpU8(0, 145, local, TLockPulseEnd - TLockStart)
                                              : lerpU8(145, 40, t - TLockPulseEnd, TRevealEnd - TLockPulseEnd);
  setHidden(_verticalGlow, false);
  lv_obj_set_pos(_verticalGlow, CenterX - 14, CenterY - 14);
  lv_obj_set_size(_verticalGlow, 28, 28);
  lv_obj_set_style_bg_color(_verticalGlow, C_GLOW, 0);
  lv_obj_set_style_bg_opa(_verticalGlow, glowOpa, 0);
  setHidden(_verticalCore, false);
  lv_obj_set_pos(_verticalCore, CenterX - 4, CenterY - 4);
  lv_obj_set_size(_verticalCore, 8, 8);
  lv_obj_set_style_bg_color(_verticalCore, C_CORE, 0);
  lv_obj_set_style_bg_opa(_verticalCore, scanOpa, 0);

  // ---- four lock-on bracket corners --------------------------------------
  // Horizontal ticks (top-left, top-right, bottom-left, bottom-right) that
  // tighten inward as they brighten. The tiny acquire pulse makes the brackets
  // flash once as they settle around the life-number position.
  const uint8_t baseTickOpa = (t < TLockPulseEnd) ? lerpU8(0, 230, local, TLockPulseEnd - TLockStart)
                                                  : lerpU8(230, 85, t - TLockPulseEnd, TRevealEnd - TLockPulseEnd);
  const uint8_t tickOpa = clampU8(baseTickOpa + acquirePulse);
  const int16_t inset = (t < TLockPulseEnd) ? lerpI16(46, 22, local, TLockPulseEnd - TLockStart) : 22;

  for (uint8_t i = 0; i < 4; ++i) {
    setHidden(_fragments[i], false);
    lv_obj_set_style_bg_color(_fragments[i], C_CORE, 0);
    lv_obj_set_style_bg_opa(_fragments[i], tickOpa, 0);
    lv_obj_set_size(_fragments[i], 20, 3);
  }
  lv_obj_set_pos(_fragments[0], CenterX - inset - 20, CenterY - 22);  // top-left horizontal
  lv_obj_set_pos(_fragments[1], CenterX + inset, CenterY - 22);       // top-right horizontal
  lv_obj_set_pos(_fragments[2], CenterX - inset - 20, CenterY + 19);  // bottom-left horizontal
  lv_obj_set_pos(_fragments[3], CenterX + inset, CenterY + 19);       // bottom-right horizontal

  // Matching vertical ticks for each corner, reusing engine-trail/exhaust
  // rectangles. Same opacity envelope as the horizontal ticks.
  setHidden(_hGlow, false);
  setHidden(_hCore, false);
  setHidden(_exhaustGlow, false);
  setHidden(_exhaustCore, false);
  lv_obj_set_style_bg_color(_hGlow, C_CORE, 0);
  lv_obj_set_style_bg_color(_hCore, C_CORE, 0);
  lv_obj_set_style_bg_color(_exhaustGlow, C_CORE, 0);
  lv_obj_set_style_bg_color(_exhaustCore, C_CORE, 0);
  lv_obj_set_style_bg_opa(_hGlow, tickOpa, 0);
  lv_obj_set_style_bg_opa(_hCore, tickOpa, 0);
  lv_obj_set_style_bg_opa(_exhaustGlow, tickOpa, 0);
  lv_obj_set_style_bg_opa(_exhaustCore, tickOpa, 0);
  lv_obj_set_pos(_hGlow, CenterX - inset - 3, CenterY - 22);  // top-left vertical
  lv_obj_set_size(_hGlow, 3, 12);
  lv_obj_set_pos(_hCore, CenterX + inset, CenterY - 22);  // top-right vertical
  lv_obj_set_size(_hCore, 3, 12);
  lv_obj_set_pos(_exhaustGlow, CenterX - inset - 3, CenterY + 10);  // bottom-left vertical
  lv_obj_set_size(_exhaustGlow, 3, 12);
  lv_obj_set_pos(_exhaustCore, CenterX + inset, CenterY + 10);  // bottom-right vertical
  lv_obj_set_size(_exhaustCore, 3, 12);
}

// =============================================================================
// Hold-to-skip
// =============================================================================

void StartupIntro::notifyHoldStart() {
  // Only latch the start time on the first call; ignore while already tracking.
  if (_holdStartAt == 0) {
    _holdStartAt = lv_tick_get();
  }
}

void StartupIntro::notifyHoldEnd() {
  // Finger lifted — reset. The arc gets hidden on the next drawSkipArc() tick.
  _holdStartAt = 0;
}

void StartupIntro::drawSkipArc() {
  if (!_skipArc) return;

  if (_holdStartAt == 0) {
    // No hold in progress — keep the arc invisible and reset its angle so
    // the next hold starts from a clean state.
    setHidden(_skipArc, true);
    lv_arc_set_angles(_skipArc, 0, 0);
    return;
  }

  const uint32_t elapsed = lv_tick_elaps(_holdStartAt);

  // Compute fill angle (0 → 360° over TSkipHold ms), clamped below 360
  // so lv_arc never sees start == end which would render as full-circle.
  const uint16_t deg = (elapsed >= TSkipHold) ? 359 : static_cast<uint16_t>(359UL * elapsed / TSkipHold);

  lv_arc_set_angles(_skipArc, 0, deg);
  setHidden(_skipArc, false);

  if (elapsed >= TSkipHold) {
    // Arc fully closed → skip. finish() tears down the LVGL tree
    // (including _skipArc) and deletes this. Nothing may run after.
    finish();
  }
}

// =============================================================================
// Helpers
// =============================================================================

void StartupIntro::hideAllObjects() {
  // Called at the top of every tick(). Each phase opts back in for what it
  // wants to draw, so unused objects naturally stay invisible.
  setHidden(_fighter, true);
  setHidden(_verticalGlow, true);
  setHidden(_verticalCore, true);
  setHidden(_exhaustGlow, true);
  setHidden(_exhaustCore, true);
  for (auto* spark : _exhaustSpark)
    setHidden(spark, true);
  for (auto* swoosh : _swoosh)
    setHidden(swoosh, true);
  setHidden(_hGlow, true);
  setHidden(_hCore, true);
  setHidden(_scanline, true);
  for (auto* fragment : _fragments)
    setHidden(fragment, true);
  for (auto* icon : _icons) {
    setHidden(icon, true);
    lv_obj_set_style_text_opa(icon, 0, 0);
  }
}

void StartupIntro::setHidden(lv_obj_t* obj, bool hidden) {
  if (!obj) return;

  const bool isHidden = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
  if (isHidden == hidden) return;

  if (hidden)
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void StartupIntro::applyIconStyle(lv_obj_t* icon, lv_color_t color, uint8_t textOpa) {
  if (!icon) return;

  lv_obj_set_style_text_color(icon, color, 0);
  lv_obj_set_style_text_opa(icon, textOpa, 0);

  // Character glyphs should have no backplate, frame, or colored shadow box.
  // The transition glow comes from the scanline + fragment dots instead.
  lv_obj_set_style_bg_opa(icon, LV_OPA_0, 0);
  lv_obj_set_style_border_width(icon, 0, 0);
  lv_obj_set_style_outline_width(icon, 0, 0);
  lv_obj_set_style_shadow_width(icon, 0, 0);
  lv_obj_set_style_shadow_spread(icon, 0, 0);
  lv_obj_set_style_shadow_opa(icon, LV_OPA_0, 0);
}

void StartupIntro::timerThunk(lv_timer_t* timer) {
  auto* intro = static_cast<StartupIntro*>(lv_timer_get_user_data(timer));
  if (intro) intro->tick();
}
