#pragma once

// =============================================================================
// StartupIntro.h
//
// Short cinematic that plays when the device boots into the life-counter UI.
// It is purely a visual: it owns no game state and produces no side effects
// outside its own LVGL subtree.
//
// HIGH-LEVEL STORY (~7.1 s total)
//
//   0.00 - 0.72 s   Starfield twinkles in.
//   0.72 - 2.30 s   X-Wing rises from below, slows mid-screen, then boosts
//                   up and off the top, leaving twin red engine trails and
//                   a soft engine cone above the future horizontal line.
//   2.30 - 3.10 s   Horizontal line emerges from the cone base and pushes
//                   out toward the screen edges; cone dissolves downward.
//   3.10 - 3.28 s   Brief black beat.
//   3.28 - 6.24 s   Character parade: Boba -> Lea -> Vader -> R2-D2,
//                   joined by alternating left/right vertical scan lines
//                   in each character's accent colour.
//   6.24 - 6.36 s   R2 fades out; the screen is fully black again.
//   6.36 - 7.14 s   Blue lock-on brackets pulse around the centre while
//                   the black overlay fades, revealing the real game UI.
//
// WHEN IT PLAYS
//
//   The intro is shown on cold boot and on wake from a *manual* deep sleep
//   (chosen from the radial menu), but NOT on wake from the idle-timeout
//   chain. The policy lives in `shouldShowFor()` so the .ino doesn't have
//   to know the rules.
// =============================================================================

#include <Arduino.h>
#include <lvgl.h>

#ifndef ENABLE_STARTUP_INTRO
#define ENABLE_STARTUP_INTRO 1
#endif

class StartupIntro {
public:
  using FinishedCallback = void (*)(void* userData);
  using RevealCallback = void (*)(void* userData);

  // Why the device is entering setup(). The .ino computes this from the
  // ESP32 wake cause plus the SleepReason we stored before sleeping.
  enum class BootReason {
    ColdBoot,             // power-on (or reset). Intro plays.
    WakeFromManualSleep,  // user picked Sleep from the radial menu. Intro plays.
    WakeFromIdleSleep,    // backlight idle chain reached DEEP_SLEEP_MS. Intro does NOT play.
  };

  // Policy gate. The boot path should consult this before constructing the
  // intro; the intro itself doesn't bother to play on a wake that should be
  // silent. Returns false (skip) for `WakeFromIdleSleep` and when
  // `ENABLE_STARTUP_INTRO` is compiled out.
  static bool shouldShowFor(BootReason reason);

  // Construct + start the animation. Returns nullptr (and synchronously
  // invokes `onFinished`) when the intro is compiled out, so the caller's
  // post-intro chain runs regardless.
  static StartupIntro* start(lv_obj_t* parent,
                             FinishedCallback onFinished = nullptr,
                             void* userData = nullptr,
                             RevealCallback onReveal = nullptr);

  // Abort an in-flight intro early. Safe to call with a null handle; clears
  // the caller's pointer.
  static void skip(StartupIntro*& intro);

  // ---- Hold-to-skip interface --------------------------------------------
  //
  // Called from loop() based on Hardware::readTouchFingerDownRaw() polling.
  // The intro tracks hold duration internally; once TSkipHold ms of
  // uninterrupted contact is reached, a closing arc animation completes
  // and finish() fires automatically.
  void notifyHoldStart();
  void notifyHoldEnd();

  ~StartupIntro();

private:
  StartupIntro(lv_obj_t* parent, FinishedCallback onFinished, void* userData, RevealCallback onReveal);

  // ---- lifecycle -------------------------------------------------------
  void create();  // build LVGL objects, kick off the timer
  void tick();    // dispatch to phase methods based on elapsed time
  void finish();  // tear down, fire callback, delete this
  static void timerThunk(lv_timer_t* timer);

  // ---- phases ----------------------------------------------------------
  // Each phase is responsible for one visual moment of the intro. They all
  // take the elapsed-since-start time `t` in milliseconds and only act when
  // `t` falls within their window. Earlier phases always run hideAllObjects
  // at the start of `tick`, so a phase only ever needs to un-hide and
  // position the objects it actually uses.
  void hideAllObjects();                  // reset visibility before each tick
  void drawStarfield(uint32_t t);         // background twinkle (0 .. TLineEnd)
  void drawFighterPass(uint32_t t);       // X-Wing flyby + engine cone (TStarEnd .. TSweepEnd)
  void drawHorizontalSwoosh(uint32_t t);  // line emerges + cone dissolves (TSweepEnd .. TLineEnd)
  void drawIconParade(uint32_t t);        // character scan transitions (TIconStart .. TFinalHoldEnd)
  void drawLockOnAndReveal(uint32_t t);   // R2 fade + brackets + bg fade (TFinalHoldEnd .. TTotal)
  void drawSkipArc();                     // hold-to-skip ring; drawn on top, independent of t

  // ---- helpers ---------------------------------------------------------
  void setHidden(lv_obj_t* obj, bool hidden);
  void applyIconStyle(lv_obj_t* icon, lv_color_t color, uint8_t textOpa);

  static constexpr int16_t ScreenW = 240;
  static constexpr int16_t ScreenH = 240;
  static constexpr int16_t CenterX = ScreenW / 2;
  static constexpr int16_t CenterY = ScreenH / 2;

  // Phase boundary timestamps (ms since intro start). All phases compare
  // against these so the timeline is easy to retune from one place.
  static constexpr uint32_t TStarEnd = 720;     // starfield twinkle window closes
  static constexpr uint32_t TSweepEnd = 2300;   // X-Wing has exited the top
  static constexpr uint32_t TLineEnd = 3100;    // horizontal line has finished pushing out
  static constexpr uint32_t TBlackEnd = 3280;   // brief black beat ends
  static constexpr uint32_t TIconStart = 3280;  // first character (Boba) starts scanning in

  // Character scan cadence: 380 ms scan, 360 ms holds between transitions.
  static constexpr uint32_t TFirstHoldEnd = 4020;  // Boba hold ends (after LTR green scan in)
  static constexpr uint32_t TTrans0End = 4400;     // RTL white scan -> Lea
  static constexpr uint32_t THold1End = 4760;      // Lea hold ends
  static constexpr uint32_t TTrans1End = 5140;     // LTR red scan -> Vader
  static constexpr uint32_t THold2End = 5500;      // Vader hold ends
  static constexpr uint32_t TTrans2End = 5880;     // RTL blue scan -> R2-D2
  static constexpr uint32_t TFinalHoldEnd = 6240;  // R2 hold ends, fade-out begins
  static constexpr uint32_t TBlackBeatEnd = 6360;  // R2 has fully faded
  static constexpr uint32_t TLockStart = 6360;     // lock-on brackets begin appearing

  // Match the visible lock-on phase to the reset count-up.
  // 30 life * 26 ms step = ~780 ms.
  static constexpr uint32_t TLockRevealMs = 780;
  static constexpr uint32_t TRevealFadeDelay = 120;

  static constexpr uint32_t TLockPulseEnd =
      TLockStart + (TLockRevealMs * 2 / 3);                           // brackets at full intensity, start fading
  static constexpr uint32_t TRevealEnd = TLockStart + TLockRevealMs;  // black overlay fully transparent
  static constexpr uint32_t TTotal = TRevealEnd;                      // intro is done; finish() fires

  // How long the user must hold continuously to skip the intro.
  // Long enough that the brief waking touch (which triggers the deep-sleep
  // wake interrupt) can never accidentally fire the skip.
  static constexpr uint32_t TSkipHold = 900;

  struct Star {
    lv_obj_t* obj = nullptr;
    int16_t x = 0;
    int16_t y = 0;
    uint16_t delay = 0;
    uint8_t baseOpa = 0;
    uint8_t size = 1;
  };

  lv_obj_t* _parent = nullptr;
  lv_obj_t* _root = nullptr;
  lv_timer_t* _timer = nullptr;
  uint32_t _startedAt = 0;
  FinishedCallback _onFinished = nullptr;
  RevealCallback _onReveal = nullptr;
  void* _userData = nullptr;
  bool _finished = false;
  bool _revealPrepared = false;
  bool _lockRevealPrepared = false;

  // ---- LVGL object pool ------------------------------------------------
  // The intro reuses a small fixed set of objects across phases (e.g. the
  // _swoosh array doubles as engine-trail bands during the X-Wing pass and
  // as cone layers during the horizontal swoosh; _fragments are scan-line
  // accent dots during the parade and lock-on tick marks at the end).
  // hideAllObjects() at the top of each tick is what makes that safe.
  Star _stars[16];
  lv_obj_t* _fighter = nullptr;
  lv_obj_t* _verticalGlow = nullptr;
  lv_obj_t* _verticalCore = nullptr;
  lv_obj_t* _exhaustGlow = nullptr;
  lv_obj_t* _exhaustCore = nullptr;
  lv_obj_t* _exhaustSpark[4] = {nullptr, nullptr, nullptr, nullptr};
  lv_obj_t* _swoosh[9] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  lv_obj_t* _hGlow = nullptr;
  lv_obj_t* _hCore = nullptr;
  lv_obj_t* _scanline = nullptr;
  lv_obj_t* _fragments[4] = {nullptr, nullptr, nullptr, nullptr};
  lv_obj_t* _icons[4] = {nullptr, nullptr, nullptr, nullptr};

  // ---- Hold-to-skip state -----------------------------------------------
  // Managed independently of the animation objects above; not touched by
  // hideAllObjects(). _holdStartAt is 0 while no hold is in progress.
  uint32_t _holdStartAt = 0;
  lv_obj_t* _skipArc = nullptr;
};
