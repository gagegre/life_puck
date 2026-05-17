// FlashManager.h
//
// Six feedback arc sprites that briefly light up around the rim each
// time a life change occurs. The arc's position (top vs bottom, left
// vs right half) follows the tap location and the colour follows the
// in-game meaning of the change (green = heal, red = damage), so the
// flash reads as both a confirmation of the tap and a hint at its
// effect.
//
// There are six arcs because we need a separate sprite for each of:
//   1P top / 1P bottom
//   2P P1 top / P1 bottom (left half, clipped at the divider)
//   2P P2 top / P2 bottom (right half, clipped at the divider)

#pragma once

#include "Config.h"

#include <lvgl.h>

class FlashManager {
public:
  void begin(lv_obj_t* parent);

  // Fire a directional flash arc.
  //
  //   isTop:     true = arc on the tapped (top) half, false = bottom.
  //              Always follows tap position, never inverted by mode.
  //   isHealing: true = green (this change improved the player's state),
  //              false = red (this change worsened it).
  //
  // Decoupling these two means a count-up tap on top (which adds damage)
  // flashes the TOP arc in RED — visually consistent with both the tap
  // location and the meaning of the change.
  void trigger(bool isTop, bool isHealing, bool isP2, bool twoPlayerMode);

  // Per-loop tick: fade out any active flashes.
  void update();

  // Shrink flash arcs away from the battery arc endpoints when the
  // battery ring is visible, so the ring stays readable mid-flash.
  // Call setContracted(battery.shouldShow()) each loop iteration.
  void setContracted(bool contracted);

private:
  struct Flash {
    bool active = false;
    uint32_t at = 0;
  };

  bool _contracted = false;
  lv_obj_t *_arc1PPlus = nullptr, *_arc1PMinus = nullptr;
  lv_obj_t *_arc2PP1Plus = nullptr, *_arc2PP1Minus = nullptr;
  lv_obj_t *_arc2PP2Plus = nullptr, *_arc2PP2Minus = nullptr;
  Flash _f1PPlus, _f1PMinus;
  Flash _f2PP1Plus, _f2PP1Minus;
  Flash _f2PP2Plus, _f2PP2Minus;

  static void styleArc(lv_obj_t* arc, lv_color_t color, int s, int e, int width);
  static void makeArc(lv_obj_t*& arc, lv_color_t color, int s, int e, lv_obj_t* parent, int width);
  static lv_obj_t* makeClip(lv_obj_t* parent, int x, int y, int w, int h);
  static void makeClippedArc(lv_obj_t*& arc,
                             lv_color_t color,
                             int s,
                             int e,
                             lv_obj_t* parent,
                             int width,
                             int clipX,
                             int clipY,
                             int clipW,
                             int clipH);
  static void startFlash(Flash& f, lv_obj_t* arc);
  static void fade(Flash& f, lv_obj_t* arc, uint32_t now);
};
