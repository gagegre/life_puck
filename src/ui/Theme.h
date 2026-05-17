// Theme.h
//
// Semantic UI palette for the Life Puck. Config.h still owns low-level
// constants and legacy COLOR_* names; this layer gives newer UI code a more
// readable vocabulary and a single place to evolve the datapad/SWU look.

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
static const lv_color_t Reset = COLOR_MINUS;
static const lv_color_t Undo = COLOR_MENU_ORANGE;
static const lv_color_t ResetTrack = lv_color_hex(0x1A0000);
static const lv_color_t UndoTrack = lv_color_hex(0x1F1200);
}  // namespace Confirm
}  // namespace Theme
