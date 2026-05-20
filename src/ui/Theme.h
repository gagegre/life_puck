// Theme.h
//
// Semantic UI palette and visual constants for the Life Puck. Config.h still
// owns low-level constants and legacy COLOR_* names; this layer gives UI code a
// more readable vocabulary and a single place to evolve the datapad/SWU look.

#pragma once

#include "Config.h"

namespace Theme {
namespace Core {
static const lv_color_t Background = COLOR_BG;
static const lv_color_t Foreground = COLOR_FG;
static const lv_color_t Muted = COLOR_VALUE_GREY;
static const lv_color_t Divider = COLOR_DIVIDER;
static const lv_color_t RingTrack = COLOR_RING_BG;
}  // namespace Core

namespace Game {
static const lv_color_t Heal = COLOR_PLUS;
static const lv_color_t Damage = COLOR_MINUS;
static const lv_color_t Warning = COLOR_BAT_YELLOW;
static const lv_color_t Critical = COLOR_MINUS;

// Sub-label colour states for the "damage/base" secondary label.
static const lv_color_t BumpFeedback = lv_color_hex(0x555555);  // rejected-input counter flash
static const lv_color_t ZeroDamage  = lv_color_hex(0x444444);  // sub-label when no damage taken

// Counter layout offsets (px). The delta badge is anchored separately.
static constexpr int SubLabelDy   = 52;  // 1P: sub-label below counter centre
static constexpr int SubLabelDy2P = 46;  // 2P: tighter to stay inside the bezel
static constexpr int CounterOy2P  = 40;  // 2P: counter offset from screen centre
}  // namespace Game

namespace Menu {
static const lv_color_t Players = COLOR_MENU_BLUE;
static const lv_color_t Count = COLOR_MENU_ORANGE;
static const lv_color_t Battery = COLOR_MENU_PINK;
static const lv_color_t Brightness = COLOR_MENU_YELLOW;
static const lv_color_t Sleep = COLOR_MENU_SLEEP;
static const lv_color_t BaseLife = COLOR_MENU_ORANGE;
static const lv_color_t Progress = lv_color_hex(0xEAF6FF);
}  // namespace Menu

namespace Confirm {
static const lv_color_t Reset      = COLOR_MINUS;
static const lv_color_t ResetTrack = lv_color_hex(0x1A0000);
static const char* const ResetIcon = FA_ICON_RESET;

static const lv_color_t Undo       = COLOR_MENU_ORANGE;
static const lv_color_t UndoTrack  = lv_color_hex(0x1F1200);
static const char* const UndoIcon  = FA_ICON_UNDO;

static const lv_color_t SkipArc    = lv_color_hex(0xE8F6FF);
static const lv_color_t SkipTrack  = lv_color_hex(0x2C5F82);
}  // namespace Confirm

namespace Divider {
// 2P horizontal centre divider geometry.
static constexpr int Width  = 120;
static constexpr int Height = 3;
static constexpr uint8_t BgOpa = LV_OPA_80;
}  // namespace Divider

namespace Flash {
// Arc stroke widths for the rim flash feedback.
static constexpr int Arc1PWidth = 48;  // 1P half-circle arcs
static constexpr int Arc2PWidth = 34;  // 2P quadrant arcs
}  // namespace Flash

namespace Toast {
// Modal confirmation disc shown after mode/settings changes.
static constexpr int CircleDiam   = 120;
static constexpr int IconOffsetY  = -22;  // upper half of circle
static constexpr int TitleOffsetY = 22;   // lower half of circle
static constexpr int TitleMaxW    = 96;   // fits inside circle minus padding
static constexpr uint32_t DurationMs = 750;
static constexpr uint8_t DimOpa      = 220;  // ~86% black overlay
static constexpr uint8_t CircleBgOpa = 50;   // soft tint, not solid
}  // namespace Toast

namespace Defeat {
// "BASE LOST" overlay geometry and animation constants.
static constexpr int PanelDiam      = 184;
static constexpr int TitleOffsetY   = 0;
static constexpr int PulseMidDiam   = 194;
static constexpr int PulseInnerDiam = 150;

static constexpr uint32_t PulseMs = 1350;

// Outside-in breathing pulse: bg and border max opacities per ring.
static constexpr uint8_t PulseBgOuter     = 86;
static constexpr uint8_t PulseBgMid       = 52;
static constexpr uint8_t PulseBgInner     = 26;
static constexpr uint8_t PulseBorderOuter = 120;
static constexpr uint8_t PulseBorderMid   = 80;
static constexpr uint8_t PulseBorderInner = 46;

static constexpr uint8_t DimOpa         = 218;
static constexpr uint8_t PanelBgOpa     = 42;   // initial panel fill before breathing starts
static constexpr uint8_t PanelBreathBase = 34;  // breathing floor opacity
static constexpr uint8_t PanelBreathWave = 20;  // breathing wave amplitude
}  // namespace Defeat
}  // namespace Theme
