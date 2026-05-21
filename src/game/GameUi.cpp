// GameUi.cpp
//
// Method bodies for GameUi. See GameUi.h for the interface.

#include "GameUi.h"
#include "Battery.h"
#include "FlashManager.h"
#include "Theme.h"

void GameUi::begin(lv_obj_t* parent, FlashManager* flash, Battery* battery) {
  _battery = battery;

  _divider = lv_obj_create(parent);
  lv_obj_remove_style_all(_divider);
  // Horizontal divider for the across-each-other 2P layout: centred at screen
  // midline. The two counters sit above (P2) and below (P1).
  lv_obj_set_size(_divider, Theme::Divider::Width, Theme::Divider::Height);
  lv_obj_set_pos(_divider, CENTER_X - Theme::Divider::Width / 2, CENTER_Y - 1);
  lv_obj_set_style_bg_color(_divider, COLOR_DIVIDER, 0);
  lv_obj_set_style_bg_opa(_divider, Theme::Divider::BgOpa, 0);
  lv_obj_set_style_radius(_divider, 1, 0);
  lv_obj_remove_flag(_divider, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(_divider, LV_OBJ_FLAG_HIDDEN);

  _p1.begin(parent, flash, /*isP2=*/false);
  _p2.begin(parent, flash, /*isP2=*/true);
  _p2.setVisible(false);
}

void GameUi::enterTwoPlayer() {
  _twoPlayerMode = true;
  applyFonts();
  // Across-each-other layout: P1 below the divider, P2 above (rotated 180).
  _p1.centerHalf(/*topSide=*/false);
  _p2.setVisible(true);
  _p2.centerHalf(/*topSide=*/true);
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
