// FlashManager.h
//
// Six feedback arc sprites that briefly light up around the rim each
// time a life change occurs. The arc's position (left vs right half,
// or quadrant in 2P) follows the tap location and the colour follows
// the in-game meaning of the change (green = heal, red = damage), so
// the flash reads as both a confirmation of the tap and a hint at
// its effect.
//
// There are six arcs because we need a separate sprite for each of:
//   1P: left / right half-circle arcs (right = + tap, left = - tap).
//   2P across: one arc per screen quadrant (TL, TR, BL, BR), each
//              clipped to its quadrant so the flash hugs only the
//              tapped corner. Player association:
//                P2 (top, flipped) : + tap = TL,  - tap = TR
//                P1 (bottom)       : + tap = BR,  - tap = BL

#pragma once

#include "Config.h"

#include <lvgl.h>

class FlashManager {
public:
  void begin(lv_obj_t* parent);

  // Fire a directional flash arc.
  //
  //   isPlus:    true  = the tap added life (in the player's frame),
  //              false = the tap subtracted life. Drives which screen
  //              quadrant lights up (see header for the mapping).
  //   isHealing: true = green (this change improved the player's state),
  //              false = red (this change worsened it).
  //
  // Decoupling these two means a count-up + tap (which adds damage)
  // flashes the "+ quadrant" in RED -- visually consistent with both
  // the tap location and the meaning of the change.
  void trigger(bool isPlus, bool isHealing, bool isP2, bool twoPlayerMode);

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
  // 1P half-circle arcs, named by their screen position. Right arc is
  // the + tap target, left arc the - tap target.
  lv_obj_t *_arc1PRight = nullptr, *_arc1PLeft = nullptr;
  // 2P-across quadrant arcs, named by their screen position.
  lv_obj_t *_arcTL = nullptr, *_arcTR = nullptr;
  lv_obj_t *_arcBL = nullptr, *_arcBR = nullptr;
  Flash _f1PRight, _f1PLeft;
  Flash _fTL, _fTR, _fBL, _fBR;

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
