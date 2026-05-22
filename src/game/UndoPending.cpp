// UndoPending.cpp
//
// Orange confirmation ring used by swipe-to-arm undo. It intentionally
// mirrors ResetPendingOverlay so reset and undo share the same mental model:
// an armed action is only executed after a deliberate centre hold.

#include "UndoPending.h"

void UndoPendingOverlay::begin(lv_obj_t* parent) {
  _dwell.begin(parent, OuterRingDwell::Style::Undo);
}

void UndoPendingOverlay::show() {
  _dwell.show();
}

void UndoPendingOverlay::hide()               { _dwell.hide(); }
void UndoPendingOverlay::setProgress(float p) { _dwell.setProgress(p); }
