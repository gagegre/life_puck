// FlashManager.h
//
// Two feedback arc sprites that briefly light up around the rim each
// time a life change occurs. The arc's position follows the tap location
// (top half for +, bottom half for -) and the colour follows the in-game
// meaning of the change (green = heal, red = damage), so the flash reads
// as both a confirmation of the tap and a hint at its effect.

#pragma once

#include "Theme.h"

#include <lvgl.h>

class FlashManager {
public:
  void begin(lv_obj_t* parent);

  // Fire a directional flash arc.
  //
  //   isPlus:    true  = the tap added life (in the player's frame),
  //              false = the tap subtracted life. Drives which screen
  //              half lights up.
  //   isHealing: true = green, false = red. Decoupled from isPlus so
  //              count-up + tap (which adds damage) flashes the "+ half"
  //              in RED.
  void trigger(bool isPlus, bool isHealing);

  // Per-loop tick: fade out any active flashes.
  void update();

  // Shrink flash arcs away from the battery arc endpoints when the
  // battery ring is visible.
  void setContracted(bool contracted);

private:
  struct Flash {
    bool active = false;
    uint32_t at = 0;
  };

  bool _contracted = false;
  lv_obj_t *_arcTop = nullptr, *_arcBot = nullptr;
  Flash _fTop, _fBot;

  static void styleArc(lv_obj_t* arc, lv_color_t color, int s, int e, int width);
  static void makeArc(lv_obj_t*& arc, lv_color_t color, int s, int e, lv_obj_t* parent, int width);
  static void startFlash(Flash& f, lv_obj_t* arc);
  static void fade(Flash& f, lv_obj_t* arc, uint32_t now);
};
