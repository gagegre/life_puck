// RadialMenu.h
//
// Single-screen settings UI for the life puck.
//
// Public surface:
//   - RadialMenu       : router that owns the overlay + five MenuView
//                        instances (top ring + four sub-views) and
//                        forwards touch input to whichever one is active.
//   - MenuHost         : abstract interface RadialMenu implements; lets
//                        views reach back to shared services (game state,
//                        backlight, battery, centre label) without each
//                        carrying a full RadialMenu*.
//   - iconForAction()  : MenuAction -> Font Awesome glyph helper used by
//                        both menu views and the top-level toast.
//
// The MenuView subclasses (TopRingView, ChoiceView, BatterySubView,
// BrightnessView, BaseSelectorView) and the segment-building helpers
// are implementation details and live entirely in RadialMenu.cpp.

#pragma once

#include "Config.h"
#include "GameUi.h"
#include <lvgl.h>

class Backlight;
class Battery;

// ---- Centre label stack (icon + title + value) ----
//
// Shared between every view; lifted out of RadialMenu so both menu views
// and other classes can hand it pre-formatted content.
struct CentreLabel {
  lv_obj_t* icon = nullptr;
  lv_obj_t* title = nullptr;
  lv_obj_t* value = nullptr;

  void build(lv_obj_t* parent);

  // Normal three-line stack (icon, title, value).
  void set(const char* iconStr, const char* titleStr, const char* valueStr);

  // Two-line stack for the centre-dead-zone affordance.
  void setClose(const char* titleStr);

  void clear();
  void setIconFA(const char* str);
  void layoutNormal();
  void layoutClose();
  void bringToFront();
};

// MenuHost is the narrow interface each MenuView reaches up to its owning
// RadialMenu for. Keeping it abstract decouples the views from the router.
class MenuHost {
public:
  virtual ~MenuHost() = default;
  virtual void requestView(int viewId) = 0;   // 0=TOP 1=CHOICE 2=BATTERY 3=BRIGHTNESS 4=BASE
  virtual void fireAction(MenuAction a) = 0;  // top-level fire-and-stay-open
  virtual void requestClose() = 0;
  virtual GameState& game() = 0;
  virtual Backlight& backlight() = 0;
  virtual Battery& battery() = 0;
  virtual CentreLabel& centre() = 0;
  virtual lv_obj_t* overlay() = 0;
  virtual void setChoiceTarget(MenuAction a) = 0;
  virtual MenuAction choiceTarget() const = 0;
};

// Forward-declared so RadialMenu can hold view pointers without exposing
// the implementations.
class MenuView;

class RadialMenu : public MenuHost {
public:
  enum ViewId : int {
    VIEW_TOP = 0,
    VIEW_CHOICE = 1,
    VIEW_BATTERY = 2,
    VIEW_BRIGHTNESS = 3,
    VIEW_BASE_SELECTOR = 4
  };

  enum class LiftResult : uint8_t { NOTHING, CLOSE_MENU };

  void begin(lv_obj_t* parent, GameState* state, GameUi* ui,
             Backlight* backlight, Battery* battery);

  bool isOpen() const {
    return _open;
  }

  void show();
  void close();

  // Called from the touch handler when a new sample arrives.
  void handleTouch(int x, int y);

  // Called when a sample is missing but the finger is still down.
  void markFingerStillDown();

  // Called when no sample is available. releaseConfirmed=true means
  // the raw I2C touch register says no finger is on the panel.
  LiftResult notifyFingerLifted(bool releaseConfirmed = false);

  // Called every loop tick while the menu is open.
  void tick();

  // Returns and clears any pending action that should fire after the menu.
  MenuAction takePendingAction();

  // After PLAYER_TOGGLE / COUNT_DIRECTION etc. mutate the game flags
  // we need to update the top-ring centre and segment icons.
  void onGameStateChanged();

  // ---- MenuHost implementation ----
  void requestView(int viewId) override;
  void fireAction(MenuAction a) override;
  void requestClose() override;
  GameState& game() override {
    return *_gameState;
  }
  Backlight& backlight() override {
    return *_backlight;
  }
  Battery& battery() override {
    return *_battery;
  }
  CentreLabel& centre() override {
    return _centre;
  }
  lv_obj_t* overlay() override {
    return _overlay;
  }
  void setChoiceTarget(MenuAction a) override {
    _choiceTarget = a;
  }
  MenuAction choiceTarget() const override {
    return _choiceTarget;
  }

private:
  lv_obj_t* _overlay = nullptr;
  CentreLabel _centre;

  MenuView* _viewTop = nullptr;
  MenuView* _viewChoice = nullptr;
  MenuView* _viewBattery = nullptr;
  MenuView* _viewBrightness = nullptr;
  MenuView* _viewBaseSelector = nullptr;

  ViewId _currentView = VIEW_TOP;
  bool _open = false;
  bool _fingerDown = false;
  bool _ignoreOpeningTouch = false;
  bool _centerTouchActive = false;
  uint32_t _centerTouchAt = 0;
  uint32_t _lastFingerSeenAt = 0;

  MenuAction _pendingAction = MenuAction::NONE;
  MenuAction _choiceTarget = MenuAction::NONE;

  GameState* _gameState = nullptr;
  GameUi* _gameUi = nullptr;
  Backlight* _backlight = nullptr;
  Battery* _battery = nullptr;

  MenuView* currentView();
  void activateView(ViewId v);
  void buildOverlay(lv_obj_t* parent);
};

// Public icon helper -- shared by menu views, executeMenuAction(), and
// related call sites that need to display the FA glyph for a given action.
const char* iconForAction(MenuAction a, const GameState& g);
