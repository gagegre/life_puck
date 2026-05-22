// GameUi.cpp
//
// Method bodies for GameUi. See GameUi.h for the interface.

#include "GameUi.h"
#include "Battery.h"
#include "FlashManager.h"
#include "Theme.h"

void GameUi::begin(lv_obj_t* parent, FlashManager* flash, Battery* battery) {
  _battery = battery;
  _p.begin(parent, flash);
}

void GameUi::hideForMenu() {
  lv_obj_add_flag(_p.lvObj(), LV_OBJ_FLAG_HIDDEN);
  _p.hideBaseReveal();
  if (_battery) _battery->setMenuOpen(true);
}

void GameUi::showAfterMenu() {
  lv_obj_remove_flag(_p.lvObj(), LV_OBJ_FLAG_HIDDEN);
  if (_battery) _battery->setMenuOpen(false);
}

void GameUi::setCounterVisible(bool visible) {
  _p.setVisible(visible);
}

void GameUi::reset(bool countUpMode) {
  _p.reset(countUpMode);
}

void GameUi::restoreValue(int life) {
  _p.setValue(life);
}
