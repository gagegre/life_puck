// Battery.h
//
// Battery monitor + LVGL widget.
//
// Visual model
//   * A 4 px edge-arc ring around the screen rim (270° sweep, 90° gap
//     at the bottom). Colour tracks level: green -> amber -> red. While
//     charging, the arc animates and turns blue.
//   * A full-screen detail overlay that can be brought up (e.g. by a
//     centre tap from the touch handler) to show percent + voltage +
//     status. Auto-hides after 3 s.

#pragma once

#include "Config.h"

#include <lvgl.h>

class Battery {
public:
  // Build all LVGL widgets; reads an initial voltage so percent() is
  // valid immediately. Pass the screen-active object (or any sensible
  // parent), the arc fills the rim, the overlay covers the screen.
  void begin(lv_obj_t* parent);

  // Per-loop tick: throttled ADC reads, charging animation, auto-hide.
  void update();

  // ---- mode / visibility -------------------------------------------------

  void setMode(BatteryMode mode);
  BatteryMode mode() const {
    return _mode;
  }

  // True if the arc widget should currently be visible (respecting mode,
  // charging state, presence detection and whether the menu is open).
  bool shouldShow() const;

  // Called by GameUi when the radial menu opens/closes; while the menu
  // is open the battery widget is always hidden.
  void setMenuOpen(bool open);

  // Discard cached values, re-read voltage immediately, and refresh widgets.
  void forceRefresh();

  // ---- detail overlay ----------------------------------------------------

  void showOverlay();
  void hideOverlay();
  bool isOverlayVisible() const {
    return _overlayVisible;
  }

  // ---- percent label toggle (small text in the arc's bottom gap) ---------
  // The detail overlay always shows the percentage regardless of this flag.

  void setShowPercent(bool on) {
    _showPercent = on;
  }
  void togglePercent() {
    _showPercent = !_showPercent;
  }
  bool isShowingPercent() const {
    return _showPercent;
  }

  // ---- introspection -----------------------------------------------------

  int percent() const {
    return _percent;
  }
  float volts() const {
    return _lastVolts;
  }
  bool isCharging() const {
    return _isCharging;
  }
  bool isUsbOnly() const {
    return _usbOnly;
  }
  bool batteryPresent() const {
    return _batteryPresent;
  }

private:
  BatteryMode _mode = BatteryMode::AUTO;
  bool _showPercent = false;
  bool _isCharging = false;
  bool _usbOnly = false;
  bool _batteryPresent = false;
  int _percent = 0;
  int _animPercent = 0;
  bool _menuOpen = false;
  float _lastVolts = 0.0f;
  uint32_t _lastReadAt = 0;
  uint32_t _lastAnimAt = 0;
  uint32_t _overlayShownAt = 0;
  bool _overlayVisible = false;

  bool _initialized = false;
  lv_obj_t* _arc = nullptr;
  lv_obj_t* _gapLabel = nullptr;  // % text in the arc's bottom gap
  lv_obj_t* _overlay = nullptr;
  lv_obj_t* _overlayCircle = nullptr;
  lv_obj_t* _overlayPct = nullptr;
  lv_obj_t* _overlayVolt = nullptr;
  lv_obj_t* _overlayStatus = nullptr;

  // ---- voltage / level helpers ------------------------------------------

  void readVoltageNow();
  void advanceChargingAnimation();
  static int voltsToPercent(float v);
  static lv_color_t levelColor(int pct);

  // ---- refresh() helpers ------------------------------------------------

  void hideAll();
  void showArc(int visualPct, lv_color_t color);
  void showPercentLabel(int pct, lv_color_t color);
  void refresh();

  // ---- widget builders --------------------------------------------------

  void createArc(lv_obj_t* parent);
  void createOverlay(lv_obj_t* parent);
};
