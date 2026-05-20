// DefeatOverlay.h
//
// Full-screen "BASE LOST" offline overlay. Triggered when a player's
// distance-to-defeat hits 0 and remains visible as long as that player
// is defeated.
//
// Visual language:
//   _dim       : darkens the screen so the life number disappears under
//                the offline state.
//   _panel     : large red-tinted rounded status surface that fills the
//                counter area.
//   _pulseOuter/_pulseMid/_pulseInner : subtle red outside-in breathing
//                pulse while the overlay is active.
//   _title     : large centered "BASE LOST" (or "Pn BASE LOST" in 2P)
//                text block.
//
// 1P and 2P: one global centered offline overlay. In 2P the title also
// identifies which player lost the base. The life counters are hidden
// by GameUi while this modal is active, so the overlay fully owns the
// screen instead of stacking over the numbers.
//
// Tapping the overlay restarts the game and hides the overlay.

#pragma once

#include "Theme.h"
#include <lvgl.h>

class DefeatOverlay {
public:

  void begin(lv_obj_t* parent, lv_obj_t* shakeTarget);

  // Trigger the offline overlay for `player` (0=P1, 1=P2).
  void show(int player, bool twoPlayer);

  void cancel();

  bool isActive() const {
    return _active;
  }
  int player() const {
    return _player;
  }

  // Drive the subtle persistent red outside-in pulse. Unlike the old
  // sequence, this does not time out; it stays until the player heals.
  void update(uint32_t now);

private:
  lv_obj_t* _parent = nullptr;
  lv_obj_t* _shakeTarget = nullptr;
  lv_obj_t* _dim = nullptr;
  lv_obj_t* _pulseOuter = nullptr;
  lv_obj_t* _pulseMid = nullptr;
  lv_obj_t* _pulseInner = nullptr;
  lv_obj_t* _panel = nullptr;
  lv_obj_t* _titleShadow = nullptr;
  lv_obj_t* _title = nullptr;

  bool _active = false;
  bool _twoPlayer = false;
  uint32_t _startedAt = 0;
  int _player = 0;
  int _cx = CENTER_X;
  int _cy = CENTER_Y;

  void configureTitleLabel(lv_obj_t* lbl);
  void setTitleText(const char* text);
  void layoutTitleLabel(lv_obj_t* lbl, int xOffset, int yOffset);
  void buildPulseLayer(lv_obj_t*& obj);
  void layoutForPlayer();
  void layoutPulse(lv_obj_t* obj, int w, int h, int radius);
  uint8_t wave(uint32_t elapsed, uint32_t offset, uint8_t maxOpa) const;
  void applyPulse(lv_obj_t* obj, uint8_t bgOpa, uint8_t borderOpa);
  void updatePulse(uint32_t elapsed);
};
