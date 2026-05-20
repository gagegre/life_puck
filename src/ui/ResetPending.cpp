// ResetPending.cpp
//
// Implementation of ResetPendingOverlay. ResetPending struct is fully
// inline in the header.

#include "ResetPending.h"

void ResetPendingOverlay::begin(lv_obj_t* parent) {
  _dwell.begin(parent, OuterRingDwell::Style::Reset);
}

void ResetPendingOverlay::show()                { _dwell.show(); }
void ResetPendingOverlay::hide()                { _dwell.hide(); }
void ResetPendingOverlay::setProgress(float p)  { _dwell.setProgress(p); }
