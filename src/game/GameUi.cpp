// GameUi.cpp
//
// Method bodies for GameUi. See GameUi.h for the interface.

#include "GameUi.h"
#include "Battery.h"
#include "FlashManager.h"

void GameUi::begin(lv_obj_t* parent, FlashManager* flash, Battery* battery) {
  _battery = battery;

  _divider = lv_obj_create(parent);
  lv_obj_remove_style_all(_divider);
  // Longer divider (120 px) for stronger visual separation in 2P mode.
  // Sits centred vertically across the screen, ending just shy of the
  // round bezel on each end so the corners don't get clipped.
  lv_obj_set_size(_divider, 3, 120);
  lv_obj_set_pos(_divider, CENTER_X - 1, CENTER_Y - 60);
  lv_obj_set_style_bg_color(_divider, COLOR_DIVIDER, 0);
  lv_obj_set_style_bg_opa(_divider, LV_OPA_80, 0);
  lv_obj_set_style_radius(_divider, 1, 0);
  lv_obj_remove_flag(_divider, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(_divider, LV_OBJ_FLAG_HIDDEN);

  _p1.begin(parent, flash, /*isP2=*/false, /*flipped=*/false);
  _p2.begin(parent, flash, /*isP2=*/true, /*flipped=*/true);
  _p2.setVisible(false);
}

void GameUi::enterTwoPlayer() {
  _twoPlayerMode = true;
  applyFonts();
  _p1.centerHalf(true);
  _p2.setVisible(true);
  _p2.centerHalf(false);
  lv_obj_remove_flag(_divider, LV_OBJ_FLAG_HIDDEN);
}

void GameUi::exitTwoPlayer() {
  _twoPlayerMode = false;
  applyFonts();
  _p1.centerFull();
  _p2.setVisible(false);
  lv_obj_add_flag(_divider, LV_OBJ_FLAG_HIDDEN);
}

void GameUi::hideForMenu() {
  lv_obj_add_flag(_p1.lvObj(), LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_p2.lvObj(), LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_divider, LV_OBJ_FLAG_HIDDEN);
  if (_battery) _battery->setMenuOpen(true);
}

void GameUi::showAfterMenu(bool twoPlayerMode) {
  lv_obj_remove_flag(_p1.lvObj(), LV_OBJ_FLAG_HIDDEN);
  if (twoPlayerMode) {
    lv_obj_remove_flag(_p2.lvObj(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_divider, LV_OBJ_FLAG_HIDDEN);
  }
  if (_battery) _battery->setMenuOpen(false);
}

void GameUi::setCountersVisible(bool visible, bool twoPlayerMode) {
  _p1.setVisible(visible);
  _p2.setVisible(visible && twoPlayerMode);
  if (twoPlayerMode && visible)
    lv_obj_remove_flag(_divider, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(_divider, LV_OBJ_FLAG_HIDDEN);
}

void GameUi::resetBoth(bool countUpMode) {
  _p1.reset(countUpMode);
  // Always reset P2 too so its base label stays in sync even when 1P is
  // active; P2's visibility is controlled separately by enter/exitTwoPlayer().
  _p2.reset(countUpMode);
}

void GameUi::restoreValues(int p1Life, int p2Life) {
  _p1.setValue(p1Life);
  _p2.setValue(p2Life);
}
