// RadialMenu.cpp
//
// Implementation of the radial menu: the router (RadialMenu), the five
// MenuView subclasses, the centre label, and the segment-builder
// helpers.
//
// Everything that doesn't need to be visible to external code (the view
// hierarchy, the segment helpers, the per-view drawing logic) lives in
// this translation unit. The .h exposes only the router and the icon
// helper.

#include "RadialMenu.h"
#include "Battery.h"
#include "Backlight.h"
#include "Clock.h"

#include <math.h>
#include <stdio.h>
#include <Arduino.h>

// ==============================================================
// iconForAction (public; declared in RadialMenu.h)
// ==============================================================

const char* iconForAction(MenuAction a, const GameState& g) {
  switch (a) {
    case MenuAction::PLAYER_TOGGLE:
      return g.twoPlayer ? FA_ICON_VERSUS : FA_ICON_SINGLE;
    case MenuAction::COUNT_DIRECTION:
      return g.countUp ? FA_ICON_COUNT_UP : FA_ICON_COUNT_DOWN;
    case MenuAction::BATTERY:
      return FA_ICON_BATTERY_THREE_QUARTERS;
    case MenuAction::BRIGHTNESS:
      return FA_ICON_BRIGHTNESS;
    case MenuAction::SLEEP:
      return FA_ICON_POWER;
    case MenuAction::SET_1P:
      return FA_ICON_SINGLE;
    case MenuAction::SET_2P:
      return FA_ICON_VERSUS;
    case MenuAction::COUNT_DOWN:
      return FA_ICON_COUNT_DOWN;
    case MenuAction::COUNT_UP:
      return FA_ICON_COUNT_UP;
    case MenuAction::SLEEP_OFF:
      return FA_ICON_POWER;
    case MenuAction::BASE_SELECTOR:
      return FA_ICON_BASE_LIFE;
    default:
      return "";
  }
}

// ==============================================================
// CentreLabel implementation
// ==============================================================

void CentreLabel::build(lv_obj_t* parent) {
  title = lv_label_create(parent);
  lv_obj_set_style_text_color(title, COLOR_FG, 0);
  lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);
  lv_label_set_text(title, "");

  value = lv_label_create(parent);
  lv_obj_set_style_text_color(value, COLOR_VALUE_GREY, 0);
  lv_obj_set_style_text_font(value, LV_FONT_DEFAULT, 0);
  lv_label_set_text(value, "");

  icon = lv_label_create(parent);
  lv_obj_set_style_text_color(icon, COLOR_FG, 0);
  lv_obj_set_style_text_font(icon, &font_awesome_icons, 0);
  lv_label_set_text(icon, "");
}

void CentreLabel::set(const char* iconStr, const char* titleStr, const char* valueStr) {
  setIconFA(iconStr ? iconStr : "");
  lv_label_set_text(title, titleStr ? titleStr : "");
  lv_label_set_text(value, valueStr ? valueStr : "");
  layoutNormal();
  bringToFront();
}

void CentreLabel::setClose(const char* titleStr) {
  setIconFA(FA_ICON_CANCEL);
  lv_label_set_text(title, titleStr ? titleStr : "");
  lv_label_set_text(value, "");
  layoutClose();
  bringToFront();
}

void CentreLabel::clear() {
  setIconFA("");
  lv_label_set_text(title, "");
  lv_label_set_text(value, "");
}

void CentreLabel::setIconFA(const char* str) {
  lv_obj_set_style_text_font(icon, &font_awesome_icons, 0);
  lv_label_set_text(icon, str);
}

void CentreLabel::layoutNormal() {
  lv_obj_align(icon, LV_ALIGN_CENTER, 0, CENTRE_ICON_Y);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, CENTRE_TITLE_Y);
  lv_obj_align(value, LV_ALIGN_CENTER, 0, CENTRE_VALUE_Y);
}

void CentreLabel::layoutClose() {
  lv_obj_align(icon, LV_ALIGN_CENTER, 0, CENTRE_CLOSE_ICON_Y);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, CENTRE_CLOSE_TITLE_Y);
  lv_obj_align(value, LV_ALIGN_CENTER, 0, 28);
}

void CentreLabel::bringToFront() {
  if (icon) lv_obj_move_foreground(icon);
  if (title) lv_obj_move_foreground(title);
  if (value) lv_obj_move_foreground(value);
}

// ==============================================================
// MenuView -- abstract base for all radial menu views
// ==============================================================

class MenuView {
public:
  explicit MenuView(MenuHost* host)
      : _host(host) {}
  virtual ~MenuView() = default;

  virtual void build() = 0;
  virtual void onEnter() = 0;
  virtual void onExit() = 0;
  virtual void onTouch(int x, int y, const PolarHit& h) = 0;
  virtual void onCentreTouch() = 0;
  virtual void onLeftCentre() {}  // optional: finger left dead-zone
  virtual void tick(uint32_t now) = 0;
  // Per-tick hook that runs only when the finger is LIFTED. Used by
  // sub-views that want to auto-commit after a short idle period.
  virtual void idleTick(uint32_t now) {
    (void)now;
  }
  virtual bool onLift() = 0;  // true = close the menu
  virtual const char* centreActionLabel() const {
    return "BACK";
  }

protected:
  MenuHost* _host;
};

// ==============================================================
// Segment-building helpers (file-local)
// ==============================================================

namespace {

struct RingSegment {
  lv_obj_t* arc = nullptr;
  lv_obj_t* iconLbl = nullptr;
  MenuAction action = MenuAction::NONE;
  const char* icon = "";
  lv_color_t color = lv_color_hex(0xFFFFFF);
};

void placeRadial(lv_obj_t* obj, float deg) {
  const float rad = deg * PI / 180.0f;
  const int R = MENU_OUTER_RADIUS - MENU_RING_THICKNESS / 2;
  const int dx = (int)lroundf(R * cosf(rad));
  const int dy = (int)lroundf(R * sinf(rad));
  lv_obj_align(obj, LV_ALIGN_CENTER, dx, dy);
}

lv_obj_t* makeRingSegment(lv_obj_t* parent, float degStart, float degEnd, lv_color_t color) {
  lv_obj_t* arc = lv_arc_create(parent);
  lv_obj_set_size(arc, SCREEN_W, SCREEN_H);
  lv_obj_center(arc);
  lv_arc_set_rotation(arc, 0);
  lv_arc_set_bg_angles(arc, (int)lroundf(degStart), (int)lroundf(degEnd));
  lv_arc_set_angles(arc, (int)lroundf(degStart), (int)lroundf(degEnd));
  lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_arc_width(arc, MENU_RING_THICKNESS, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, MENU_RING_THICKNESS, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, COLOR_RING_BG, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(arc, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  return arc;
}

lv_obj_t* makeIconLabel(lv_obj_t* parent, const char* sym, float deg) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_obj_set_style_text_color(lbl, COLOR_FG, 0);
  lv_obj_set_style_text_font(lbl, &font_awesome_icons, 0);
  lv_label_set_text(lbl, sym);
  placeRadial(lbl, deg);
  return lbl;
}

// makeTextLabel removed -- it was declared but never used in the original
// (a leftover from when ChoiceView used text rather than icons).

}  // namespace

// ==============================================================
// TopRingView -- the six-segment top-level menu
// ==============================================================

class TopRingView : public MenuView {
public:
  using MenuView::MenuView;

  static constexpr float SEGMENT_DEG = 360.0f / MENU_ACTION_COUNT;
  static constexpr float TOP_DEG = 270.0f;  // 12 o'clock in LVGL frame

  void build() override {
    static const MenuAction kActions[MENU_ACTION_COUNT] = {MenuAction::PLAYER_TOGGLE,
                                                           MenuAction::COUNT_DIRECTION,
                                                           MenuAction::BATTERY,
                                                           MenuAction::BRIGHTNESS,
                                                           MenuAction::SLEEP,
                                                           MenuAction::BASE_SELECTOR};
    static const lv_color_t kColors[MENU_ACTION_COUNT] = {
        COLOR_MENU_BLUE,
        COLOR_MENU_ORANGE,
        COLOR_MENU_PINK,
        COLOR_MENU_YELLOW,
        COLOR_MENU_SLEEP,
        COLOR_MENU_ORANGE  // BASE_SELECTOR
    };

    for (uint8_t i = 0; i < MENU_ACTION_COUNT; ++i) {
      const float centreDeg = TOP_DEG + i * SEGMENT_DEG;
      const float startDeg = centreDeg - SEGMENT_DEG / 2.0f;
      const float endDeg = centreDeg + SEGMENT_DEG / 2.0f;

      RingSegment& s = _segs[i];
      s.action = kActions[i];
      s.color = kColors[i];
      s.icon = iconForAction(s.action, _host->game());
      s.arc = makeRingSegment(_host->overlay(), startDeg, endDeg, s.color);
      s.iconLbl = makeIconLabel(_host->overlay(), s.icon, centreDeg);
    }
  }

  void onEnter() override {
    refreshIcons();
    for (auto& s : _segs) {
      lv_obj_set_style_arc_opa(s.arc, LV_OPA_90, LV_PART_MAIN);
      lv_obj_set_style_arc_opa(s.arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
      lv_obj_set_style_text_color(s.iconLbl, COLOR_FG, 0);
      lv_obj_remove_flag(s.iconLbl, LV_OBJ_FLAG_HIDDEN);
    }
    _hovered = MenuAction::NONE;
    _hoveredAt = 0;
    _committed = false;
    renderCentre();
  }

  void onExit() override {
    for (auto& s : _segs) {
      lv_obj_set_style_arc_opa(s.arc, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_arc_opa(s.arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
      lv_obj_add_flag(s.iconLbl, LV_OBJ_FLAG_HIDDEN);
    }
  }

  void onTouch(int x, int y, const PolarHit& h) override {
    (void)x;
    (void)y;
    const MenuAction a = actionAt(h);
    if (a != _hovered) setHover(a);
  }

  void onCentreTouch() override {
    if (_hovered != MenuAction::NONE) {
      _hovered = MenuAction::NONE;
      _hoveredAt = 0;
      _committed = false;
      redrawHighlight();
    }
  }

  void tick(uint32_t now) override {
    (void)now;
    if (_committed || _hovered == MenuAction::NONE) return;

    // BRIGHTNESS opens its slider after the same dwell.
    if (_hovered == MenuAction::BRIGHTNESS) {
      if (Clock::elapsed(_hoveredAt, MENU_DWELL_REVEAL_MS)) {
        _host->requestView(3);  // BRIGHTNESS
      }
      return;
    }

    // BATTERY opens its sub-radial after the same dwell.
    if (_hovered == MenuAction::BATTERY) {
      if (Clock::elapsed(_hoveredAt, MENU_DWELL_REVEAL_MS)) {
        _host->requestView(2);  // BATTERY
      }
      return;
    }

    // SLEEP must always go through the confirmation dial. Never sleep
    // directly from the top-level ring, even on dwell.
    if (_hovered == MenuAction::SLEEP) {
      if (Clock::elapsed(_hoveredAt, MENU_DWELL_REVEAL_MS)) {
        _host->setChoiceTarget(MenuAction::SLEEP);
        _host->requestView(1);  // CHOICE / confirm sleep
      }
      return;
    }

    // BASE SELECTOR opens its dedicated granular-arc view on dwell.
    if (_hovered == MenuAction::BASE_SELECTOR) {
      if (Clock::elapsed(_hoveredAt, MENU_DWELL_REVEAL_MS)) {
        _host->requestView(4);  // VIEW_BASE_SELECTOR
      }
      return;
    }

    // Everything else commits after the longer dwell.
    if (Clock::elapsed(_hoveredAt, MENU_DWELL_COMMIT_MS)) {
      _committed = true;
      _host->fireAction(_hovered);
    }
  }

  bool onLift() override {
    // Quick release on a hovered parent fires the cycle/short action.
    if (_hovered == MenuAction::NONE) return true;
    switch (_hovered) {
      case MenuAction::PLAYER_TOGGLE:
      case MenuAction::COUNT_DIRECTION:
        _host->fireAction(_hovered);
        break;
      case MenuAction::SLEEP:
        _host->setChoiceTarget(MenuAction::SLEEP);
        _host->requestView(1);  // confirm dial; never sleep directly
        return false;
      case MenuAction::BATTERY:
        _host->fireAction(MenuAction::BATTERY_CYCLE);
        break;
      case MenuAction::BRIGHTNESS:
        _host->fireAction(MenuAction::BRIGHTNESS_CYCLE);
        break;
      case MenuAction::BASE_SELECTOR:
        _host->requestView(4);  // VIEW_BASE_SELECTOR
        return false;           // keep menu open
      default:
        break;
    }
    return true;
  }

  void refreshIcons() {
    for (auto& s : _segs) {
      s.icon = iconForAction(s.action, _host->game());
      lv_label_set_text(s.iconLbl, s.icon);
    }
  }

private:
  RingSegment _segs[MENU_ACTION_COUNT];
  MenuAction _hovered = MenuAction::NONE;
  uint32_t _hoveredAt = 0;
  bool _committed = false;

  MenuAction actionAt(const PolarHit& h) {
    if (!h.inRing) return MenuAction::NONE;
    float rel = h.deg - (TOP_DEG - SEGMENT_DEG / 2.0f);
    rel = normalizeDeg(rel);
    const int idx = (int)(rel / SEGMENT_DEG);
    if (idx < 0 || idx >= MENU_ACTION_COUNT) return MenuAction::NONE;
    return _segs[idx].action;
  }

  void setHover(MenuAction a) {
    _hovered = a;
    _hoveredAt = Clock::now();
    _committed = false;
    redrawHighlight();
    renderCentre();
  }

  void redrawHighlight() {
    for (auto& s : _segs) {
      const bool on = (s.action == _hovered);
      lv_obj_set_style_arc_opa(s.arc, LV_OPA_90, LV_PART_MAIN);
      lv_obj_set_style_arc_opa(s.arc, on ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_INDICATOR);
      lv_obj_set_style_text_color(s.iconLbl, on ? s.color : COLOR_FG, 0);
    }
  }

  void renderCentre() {
    CentreLabel& c = _host->centre();
    GameState& g = _host->game();
    char buf[16];

    switch (_hovered) {
      case MenuAction::PLAYER_TOGGLE:
        c.set(iconForAction(MenuAction::PLAYER_TOGGLE, g),
              UiText::PLAYERS,
              g.twoPlayer ? UiText::VERSUS : UiText::SINGLE);
        break;
      case MenuAction::COUNT_DIRECTION:
        c.set(iconForAction(MenuAction::COUNT_DIRECTION, g), UiText::COUNT, g.countUp ? UiText::UP : UiText::DOWN);
        break;
      case MenuAction::BATTERY: {
        Battery& b = _host->battery();
        char pctBuf[8], voltBuf[16];
        snprintf(pctBuf, sizeof(pctBuf), "%d%%", b.percent());
        snprintf(voltBuf, sizeof(voltBuf), "%.2fV%s", b.volts(), b.isCharging() ? " +" : "");
        c.set(FA_ICON_BATTERY_THREE_QUARTERS, pctBuf, voltBuf);
      } break;
      case MenuAction::BRIGHTNESS:
        snprintf(buf, sizeof(buf), "%d%%", _host->backlight().asPercent());
        c.set(FA_ICON_BRIGHTNESS, UiText::BRIGHTNESS, buf);
        break;
      case MenuAction::SLEEP:
        c.set(FA_ICON_POWER, UiText::SLEEP, "");
        break;
      case MenuAction::BASE_SELECTOR: {
        char baseBuf[16];
        if (g.twoPlayer)
          snprintf(baseBuf, sizeof(baseBuf), "%d | %d", g.baseLife1, g.baseLife2);
        else
          snprintf(baseBuf, sizeof(baseBuf), "%d", g.baseLife1);
        c.set(FA_ICON_BASE_LIFE, UiText::BASE_LIFE, baseBuf);
      } break;
      case MenuAction::NONE:
      default:
        c.clear();
        c.layoutNormal();
        c.bringToFront();
        break;
    }
  }
};

// ==============================================================
// ChoiceView -- bound choice sub-radial (1P vs 2P, COUNT UP vs DOWN,
// SLEEP confirm)
// ==============================================================

class ChoiceView : public MenuView {
public:
  using MenuView::MenuView;

  static constexpr uint8_t MAX_SEGS = 3;

  void build() override {
    for (uint8_t i = 0; i < MAX_SEGS; ++i) {
      RingSegment& s = _segs[i];
      s.color = COLOR_MENU_BLUE;
      s.arc = makeRingSegment(_host->overlay(), 0, 1, s.color);
      s.iconLbl = makeIconLabel(_host->overlay(), "", 0);
      lv_obj_add_flag(s.arc, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(s.iconLbl, LV_OBJ_FLAG_HIDDEN);
    }
  }

  void onEnter() override {
    _hoverIdx = -1;
    _committed = false;
    _hoveredAt = 0;
    configure(_host->choiceTarget());
    updateHighlight();
    renderCentre(-1);
  }

  void onExit() override {
    for (auto& s : _segs) {
      lv_obj_add_flag(s.arc, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(s.iconLbl, LV_OBJ_FLAG_HIDDEN);
    }
    _count = 0;
  }

  void onTouch(int x, int y, const PolarHit& h) override {
    (void)x;
    (void)y;
    const int idx = segmentAt(h);
    if (idx != _hoverIdx) {
      _hoverIdx = idx;
      _hoveredAt = Clock::now();
      _committed = false;
      updateHighlight();
      renderCentre(idx);
    }
  }

  void onCentreTouch() override {
    if (_hoverIdx >= 0) {
      _hoverIdx = -1;
      _committed = false;
      updateHighlight();
    }
  }

  void tick(uint32_t now) override {
    (void)now;
  }

  // Lift on a hovered choice commits it.
  bool onLift() override {
    if (_hoverIdx >= 0 && _hoverIdx < (int)_count) {
      _host->fireAction(_values[_hoverIdx]);
    }
    return true;
  }

private:
  RingSegment _segs[MAX_SEGS];
  MenuAction _values[MAX_SEGS] = {MenuAction::NONE, MenuAction::NONE, MenuAction::NONE};
  float _centres[MAX_SEGS] = {0.0f, 0.0f, 0.0f};
  uint8_t _count = 0;
  float _segDeg = 360.0f;
  int _hoverIdx = -1;
  uint32_t _hoveredAt = 0;
  bool _committed = false;

  void configure(MenuAction parent) {
    for (uint8_t i = 0; i < MAX_SEGS; ++i) {
      _values[i] = MenuAction::NONE;
      lv_obj_add_flag(_segs[i].arc, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(_segs[i].iconLbl, LV_OBJ_FLAG_HIDDEN);
    }
    _count = 0;

    if (parent == MenuAction::PLAYER_TOGGLE) {
      _count = 2;
      addSeg(0, FA_ICON_SINGLE, MenuAction::SET_1P, COLOR_MENU_BLUE);
      addSeg(1, FA_ICON_VERSUS, MenuAction::SET_2P, COLOR_MENU_BLUE);
    } else if (parent == MenuAction::COUNT_DIRECTION) {
      _count = 2;
      addSeg(0, FA_ICON_COUNT_DOWN, MenuAction::COUNT_DOWN, COLOR_MENU_ORANGE);
      addSeg(1, FA_ICON_COUNT_UP, MenuAction::COUNT_UP, COLOR_MENU_ORANGE);
    } else if (parent == MenuAction::SLEEP) {
      _count = 1;
      addSeg(0, FA_ICON_POWER, MenuAction::SLEEP_OFF, COLOR_MENU_SLEEP);
    }
    _segDeg = (_count > 0) ? (360.0f / (float)_count) : 360.0f;
  }

  void addSeg(uint8_t idx, const char* iconStr, MenuAction value, lv_color_t color) {
    if (idx >= MAX_SEGS || _count == 0) return;

    const float seg = 360.0f / (float)_count;
    float startDeg, endDeg, centreDeg;
    if (_count == 1) {
      startDeg = 0.0f;
      endDeg = 360.0f;
      centreDeg = TopRingView::TOP_DEG;
    } else {
      startDeg = TopRingView::TOP_DEG - 90.0f + idx * seg;
      endDeg = startDeg + seg;
      centreDeg = normalizeDeg(startDeg + seg / 2.0f);
    }

    _centres[idx] = centreDeg;
    _values[idx] = value;

    RingSegment& s = _segs[idx];
    s.color = color;
    s.action = value;
    lv_arc_set_bg_angles(s.arc, (int)lroundf(startDeg), (int)lroundf(endDeg));
    lv_arc_set_angles(s.arc, (int)lroundf(startDeg), (int)lroundf(endDeg));
    lv_obj_set_style_arc_color(s.arc, COLOR_RING_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s.arc, color, LV_PART_INDICATOR);
    lv_label_set_text(s.iconLbl, iconStr);
    placeRadial(s.iconLbl, centreDeg);
    lv_obj_remove_flag(s.arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s.iconLbl, LV_OBJ_FLAG_HIDDEN);
  }

  int segmentAt(const PolarHit& h) const {
    if (!h.inRing) return -1;
    if (_count == 1) return 0;
    for (uint8_t i = 0; i < _count; ++i) {
      if (absAngleDiff(h.deg, _centres[i]) <= _segDeg / 2.0f) return i;
    }
    return -1;
  }

  void updateHighlight() {
    for (uint8_t i = 0; i < MAX_SEGS; ++i) {
      if (i >= _count) continue;
      const bool active = ((int)i == _hoverIdx);
      lv_obj_set_style_arc_opa(_segs[i].arc, LV_OPA_90, LV_PART_MAIN);
      lv_obj_set_style_arc_opa(_segs[i].arc, active ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_INDICATOR);
      lv_obj_set_style_text_color(_segs[i].iconLbl, active ? _segs[i].color : COLOR_FG, 0);
    }
  }

  void renderCentre(int idx) {
    CentreLabel& c = _host->centre();
    GameState& g = _host->game();
    const MenuAction target = _host->choiceTarget();

    if (idx < 0) {
      switch (target) {
        case MenuAction::PLAYER_TOGGLE:
          c.set(iconForAction(MenuAction::PLAYER_TOGGLE, g), UiText::PLAYERS, "");
          break;
        case MenuAction::COUNT_DIRECTION:
          c.set(iconForAction(MenuAction::COUNT_DIRECTION, g), UiText::COUNT, "");
          break;
        case MenuAction::SLEEP:
          c.set(FA_ICON_POWER, UiText::SLEEP, "");
          break;
        default:
          c.clear();
          c.layoutNormal();
          c.bringToFront();
          break;
      }
      return;
    }

    const MenuAction v = _values[idx];
    if (target == MenuAction::PLAYER_TOGGLE) {
      c.set(iconForAction(v, g), UiText::PLAYERS, (v == MenuAction::SET_2P) ? UiText::VERSUS : UiText::SINGLE);
    } else if (target == MenuAction::COUNT_DIRECTION) {
      c.set(iconForAction(v, g), UiText::COUNT, (v == MenuAction::COUNT_UP) ? UiText::UP : UiText::DOWN);
    } else if (target == MenuAction::SLEEP) {
      c.set(FA_ICON_POWER, UiText::SLEEP, UiText::BATTERY_HIDE);
    }
  }
};

// ==============================================================
// BatterySubView -- 4 segments: HIDE, AUTO, SHOW, % (toggle)
// ==============================================================

class BatterySubView : public MenuView {
public:
  using MenuView::MenuView;

  static constexpr uint8_t SEG_COUNT = 4;
  static constexpr float START_DEG = TopRingView::TOP_DEG - 90.0f;

  void build() override {
    // Icons mirror eye-visibility metaphor: slash=off, auto=smart, open=always.
    static const char* const kIcons[SEG_COUNT] = {
        FA_ICON_HIDE_MODE,  // HIDE
        FA_ICON_AUTO_MODE,  // AUTO
        FA_ICON_SHOW_MODE,  // SHOW / ALWAYS
        FA_ICON_PERCENTAGE  // SHOW % toggle
    };
    static const lv_color_t kColors[SEG_COUNT] = {COLOR_BATTERY_HIDE, COLOR_MENU_PINK, COLOR_PLUS, COLOR_BAT_YELLOW};
    const float segDeg = 360.0f / SEG_COUNT;
    for (uint8_t i = 0; i < SEG_COUNT; ++i) {
      const float a0 = START_DEG + i * segDeg;
      const float a1 = a0 + segDeg;
      RingSegment& s = _segs[i];
      s.color = kColors[i];
      s.arc = makeRingSegment(_host->overlay(), a0, a1, s.color);
      s.iconLbl = makeIconLabel(_host->overlay(), kIcons[i], a0 + segDeg / 2.0f);
      lv_obj_add_flag(s.arc, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(s.iconLbl, LV_OBJ_FLAG_HIDDEN);
    }
  }

  void onEnter() override {
    _hoverIdx = -1;
    for (auto& s : _segs) {
      lv_obj_remove_flag(s.arc, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(s.iconLbl, LV_OBJ_FLAG_HIDDEN);
    }
    updateHighlight();
    renderCentre(-1);
  }

  void onExit() override {
    for (auto& s : _segs) {
      lv_obj_add_flag(s.arc, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(s.iconLbl, LV_OBJ_FLAG_HIDDEN);
    }
  }

  void onTouch(int x, int y, const PolarHit& h) override {
    (void)x;
    (void)y;
    const int idx = segmentAt(h);
    if (idx != _hoverIdx) {
      _hoverIdx = idx;
      updateHighlight();
      renderCentre(idx);
    }
  }

  void onCentreTouch() override {
    if (_hoverIdx >= 0) {
      _hoverIdx = -1;
      updateHighlight();
    }
  }

  void tick(uint32_t now) override {
    (void)now;
  }

  bool onLift() override {
    if (_hoverIdx >= 0 && _hoverIdx < SEG_COUNT) {
      commit(_hoverIdx);
      _host->fireAction(MenuAction::BATTERY);
    }
    return true;
  }

private:
  RingSegment _segs[SEG_COUNT];
  int _hoverIdx = -1;

  int segmentAt(const PolarHit& h) const {
    if (!h.inRing) return -1;
    const float segDeg = 360.0f / SEG_COUNT;
    float rel = h.deg - START_DEG;
    rel = normalizeDeg(rel);
    const int idx = (int)(rel / segDeg);
    return (idx < 0 || idx >= SEG_COUNT) ? -1 : idx;
  }

  void commit(int idx) {
    Battery& b = _host->battery();
    if (idx == 3) {
      b.togglePercent();
    } else {
      b.setMode((BatteryMode)idx);
    }
    if (b.mode() != BatteryMode::HIDE) b.forceRefresh();
    updateHighlight();
    renderCentre(idx);
  }

  void updateHighlight() {
    Battery& b = _host->battery();
    for (uint8_t i = 0; i < SEG_COUNT; ++i) {
      const bool hovered = ((int)i == _hoverIdx);
      const bool modeActive = (i < 3) && ((int)b.mode() == i);
      const bool pctActive = (i == 3) && b.isShowingPercent();
      const bool active = modeActive || pctActive;

      if (hovered) {
        lv_obj_set_style_arc_color(_segs[i].arc, COLOR_RING_BG, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(_segs[i].arc, LV_OPA_90, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(_segs[i].arc, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_text_color(_segs[i].iconLbl, _segs[i].color, 0);
      } else if (active) {
        // Outline: tint the track itself in segment colour, no indicator fill.
        lv_obj_set_style_arc_color(_segs[i].arc, _segs[i].color, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(_segs[i].arc, LV_OPA_40, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(_segs[i].arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_text_color(_segs[i].iconLbl, _segs[i].color, 0);
      } else {
        lv_obj_set_style_arc_color(_segs[i].arc, COLOR_RING_BG, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(_segs[i].arc, LV_OPA_90, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(_segs[i].arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_text_color(_segs[i].iconLbl, COLOR_FG, 0);
      }
    }
  }

  void renderCentre(int idx) {
    CentreLabel& c = _host->centre();
    static const char* const kModeIcons[3] = {FA_ICON_HIDE_MODE, FA_ICON_AUTO_MODE, FA_ICON_SHOW_MODE};
    static const char* const kModeNames[3] = {UiText::BATTERY_HIDE, UiText::BATTERY_AUTO, UiText::BATTERY_SHOW};
    if (idx == 3) {
      c.set(FA_ICON_PERCENTAGE,
            UiText::SHOW_PERCENT,
            _host->battery().isShowingPercent() ? UiText::STATE_ENABLED : UiText::STATE_DISABLED);
    } else if (idx >= 0 && idx < 3) {
      c.set(kModeIcons[idx], UiText::BATTERY, kModeNames[idx]);
    } else {
      // No hover: show current mode as the idle state.
      const int m = (int)_host->battery().mode();
      c.set(kModeIcons[m], UiText::BATTERY, kModeNames[m]);
    }
  }
};

// ==============================================================
// BrightnessView -- full-circle stepped brightness selector
// ==============================================================

class BrightnessView : public MenuView {
public:
  using MenuView::MenuView;

  void build() override {
    _arc = lv_arc_create(_host->overlay());
    lv_obj_set_size(_arc, SCREEN_W, SCREEN_H);
    lv_obj_center(_arc);
    lv_arc_set_rotation(_arc, 270);
    lv_arc_set_bg_angles(_arc, 0, 360);
    lv_arc_set_range(_arc, 0, 100);
    lv_obj_set_style_opa(_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_arc_width(_arc, MENU_RING_THICKNESS, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_arc, MENU_RING_THICKNESS, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_arc, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_color(_arc, COLOR_MENU_YELLOW, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(_arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(_arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_remove_flag(_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_arc, LV_OBJ_FLAG_HIDDEN);

    buildTicks();
  }

  void onEnter() override {
    const int pct = snapBrightnessPercent(_host->backlight().asPercent());
    lv_arc_set_value(_arc, pct);
    lv_obj_remove_flag(_arc, LV_OBJ_FLAG_HIDDEN);
    showTicks(true);
    updateTicks(pct);
    renderCentre(pct);
    _hasChanged = false;
    _liftedAt = 0;
    _lastCommittedPct = pct;
  }

  void onExit() override {
    lv_obj_add_flag(_arc, LV_OBJ_FLAG_HIDDEN);
    showTicks(false);
  }

  void onTouch(int x, int y, const PolarHit& h) override {
    (void)h;
    const int dx = x - CENTER_X;
    const int dy = y - CENTER_Y;
    if (dx * dx + dy * dy < MENU_HIT_INNER_RADIUS * MENU_HIT_INNER_RADIUS) return;
    // 12 o'clock = 0, increases clockwise.
    float deg = atan2f((float)dy, (float)dx) * 180.0f / PI;
    deg += 90.0f;
    deg = normalizeDeg(deg);
    const int rawPct = constrain((int)lroundf(deg / 360.0f * 100.0f), 0, 100);
    const int pct = snapBrightnessPercent(rawPct);
    _host->backlight().setPercent(pct);
    lv_arc_set_value(_arc, pct);
    updateTicks(pct);
    renderCentre(pct);

    if (pct != _lastCommittedPct) {
      _hasChanged = true;
      _lastCommittedPct = pct;
    }
    _liftedAt = 0;
  }

  void onCentreTouch() override {
    // Explicit confirm via centre tap: persist and return to top ring.
    _host->fireAction(MenuAction::BRIGHTNESS);
    _host->requestView(0);  // VIEW_TOP
    _hasChanged = false;
    _liftedAt = 0;
  }
  void tick(uint32_t now) override {
    (void)now;
  }
  bool onLift() override {
    if (_hasChanged) _liftedAt = Clock::now();
    return false;
  }
  // Auto-commit after a brief idle period.
  void idleTick(uint32_t now) override {
    if (!_hasChanged || _liftedAt == 0) return;
    if ((now - _liftedAt) < MENU_AUTO_COMMIT_IDLE_MS) return;
    _hasChanged = false;
    _liftedAt = 0;
    _host->fireAction(MenuAction::BRIGHTNESS);
    _host->requestClose();
  }
  const char* centreActionLabel() const override {
    return "CONFIRM";
  }

private:
  lv_obj_t* _arc = nullptr;
  lv_obj_t* _ticks[sizeof(BRIGHTNESS_STEPS) / sizeof(BRIGHTNESS_STEPS[0])] = {};
  bool _hasChanged = false;
  uint32_t _liftedAt = 0;
  int _lastCommittedPct = 0;

  void buildTicks() {
    constexpr int count = sizeof(BRIGHTNESS_STEPS) / sizeof(BRIGHTNESS_STEPS[0]);
    for (int i = 0; i < count; ++i) {
      _ticks[i] = lv_obj_create(_host->overlay());
      lv_obj_remove_style_all(_ticks[i]);
      positionTick(i, BRIGHTNESS_STEP_TICK_SIZE);
      lv_obj_set_style_radius(_ticks[i], LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(_ticks[i], COLOR_MENU_YELLOW, 0);
      lv_obj_set_style_bg_opa(_ticks[i], LV_OPA_70, 0);
      lv_obj_set_style_border_width(_ticks[i], 0, 0);
      lv_obj_remove_flag(_ticks[i], LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(_ticks[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  void positionTick(int i, int size) {
    const int pct = BRIGHTNESS_STEPS[i];
    const int r = MENU_OUTER_RADIUS - MENU_RING_THICKNESS / 2;
    const float rad = ((float)pct / 100.0f * 360.0f - 90.0f) * PI / 180.0f;
    const int cx = CENTER_X + (int)lroundf(cosf(rad) * r);
    const int cy = CENTER_Y + (int)lroundf(sinf(rad) * r);
    lv_obj_set_size(_ticks[i], size, size);
    lv_obj_set_pos(_ticks[i], cx - size / 2, cy - size / 2);
  }

  void showTicks(bool show) {
    constexpr int count = sizeof(BRIGHTNESS_STEPS) / sizeof(BRIGHTNESS_STEPS[0]);
    for (int i = 0; i < count; ++i) {
      if (!_ticks[i]) continue;
      if (show)
        lv_obj_remove_flag(_ticks[i], LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(_ticks[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  void updateTicks(int pct) {
    constexpr int count = sizeof(BRIGHTNESS_STEPS) / sizeof(BRIGHTNESS_STEPS[0]);
    for (int i = 0; i < count; ++i) {
      if (!_ticks[i]) continue;
      const bool active = (BRIGHTNESS_STEPS[i] == pct);
      const int size = active ? BRIGHTNESS_STEP_TICK_SIZE + 5 : BRIGHTNESS_STEP_TICK_SIZE;
      positionTick(i, size);
      lv_obj_set_style_bg_opa(_ticks[i], active ? LV_OPA_COVER : LV_OPA_70, 0);
    }
  }

  void renderCentre(int pct) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    _host->centre().set(FA_ICON_BRIGHTNESS, UiText::BRIGHTNESS, buf);
  }
};

// ==============================================================
// BaseSelectorView -- granular arc selector for base max life (24..35).
//
// Layout
//   1P  full-circle arc (like BrightnessView) - 12 tick marks
//   2P  two 120 deg half-arcs with 60 deg gap at top & bottom:
//         P1  right side  300 deg -> 60 deg  (clockwise through 0 deg)
//         P2  left  side  120 deg -> 240 deg
//   Each arc shows 12 tick dots; the selected value appears
//   large in the centre label.
// ==============================================================

class BaseSelectorView : public MenuView {
public:
  using MenuView::MenuView;

  static constexpr int VAL_MIN = 24;
  static constexpr int VAL_MAX = 35;
  static constexpr int VAL_COUNT = VAL_MAX - VAL_MIN + 1;  // 12

  // 1P arc
  static constexpr float FULL_ROT = 270.0f;
  static constexpr float FULL_SPAN = 360.0f;

  // 2P arcs (each 120 deg, 60 deg gap at top & bottom).
  static constexpr float P1_ROT = 300.0f;
  static constexpr float P1_SPAN = 120.0f;
  static constexpr float P2_ROT = 120.0f;
  static constexpr float P2_SPAN = 120.0f;

  static constexpr int TICK_SIZE_NORM = 8;
  static constexpr int TICK_SIZE_SEL = 13;

  void build() override {
    lv_obj_t* ov = _host->overlay();

    _arc1P = makeArc(ov);
    lv_arc_set_rotation(_arc1P, (int)FULL_ROT);
    lv_arc_set_bg_angles(_arc1P, 0, 360);

    _arcP1 = makeArc(ov);
    lv_arc_set_rotation(_arcP1, (int)P1_ROT);
    lv_arc_set_bg_angles(_arcP1, 0, (int)P1_SPAN);

    _arcP2 = makeArc(ov);
    lv_arc_set_rotation(_arcP2, (int)P2_ROT);
    lv_arc_set_bg_angles(_arcP2, 0, (int)P2_SPAN);

    buildTicks(_ticks1P, VAL_COUNT, FULL_ROT, FULL_SPAN);
    buildTicks(_ticksP1, VAL_COUNT, P1_ROT, P1_SPAN);
    buildTicks(_ticksP2, VAL_COUNT, P2_ROT, P2_SPAN);

    hideAll();
  }

  void onEnter() override {
    const bool twoP = _host->game().twoPlayer;
    _selP1 = _host->game().baseLife1;
    _selP2 = _host->game().baseLife2;
    _origP1 = _selP1;
    _origP2 = _selP2;
    _hasChanged = false;
    _liftedAt = 0;

    if (twoP) {
      lv_obj_add_flag(_arc1P, LV_OBJ_FLAG_HIDDEN);
      showTicks(_ticks1P, false);
      lv_obj_remove_flag(_arcP1, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(_arcP2, LV_OBJ_FLAG_HIDDEN);
      showTicks(_ticksP1, true);
      showTicks(_ticksP2, true);
    } else {
      lv_obj_remove_flag(_arc1P, LV_OBJ_FLAG_HIDDEN);
      showTicks(_ticks1P, true);
      lv_obj_add_flag(_arcP1, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(_arcP2, LV_OBJ_FLAG_HIDDEN);
      showTicks(_ticksP1, false);
      showTicks(_ticksP2, false);
    }

    updateArcs();
    updateTicks();
    renderCentre();
  }

  void onExit() override {
    hideAll();
  }

  void onTouch(int x, int y, const PolarHit& h) override {
    (void)x;
    (void)y;
    if (!h.inRing) return;
    const bool twoP = _host->game().twoPlayer;
    const float deg = h.deg;

    if (!twoP) {
      float rel = normalizeDeg(deg - FULL_ROT);
      int idx = constrain((int)lroundf(rel / FULL_SPAN * (VAL_COUNT - 1)), 0, VAL_COUNT - 1);
      _selP1 = VAL_MIN + idx;
    } else {
      float relP1 = normalizeDeg(deg - P1_ROT);
      if (relP1 <= P1_SPAN) {
        int idx = constrain((int)lroundf(relP1 / P1_SPAN * (VAL_COUNT - 1)), 0, VAL_COUNT - 1);
        _selP1 = VAL_MIN + idx;
      }
      float relP2 = normalizeDeg(deg - P2_ROT);
      if (relP2 <= P2_SPAN) {
        int idx = constrain((int)lroundf(relP2 / P2_SPAN * (VAL_COUNT - 1)), 0, VAL_COUNT - 1);
        _selP2 = VAL_MIN + idx;
      }
    }

    updateArcs();
    updateTicks();
    renderCentre();

    if (_selP1 != _origP1 || _selP2 != _origP2) _hasChanged = true;
    _liftedAt = 0;
  }

  void onCentreTouch() override {
    // Explicit confirm: commit values and return to top ring.
    _host->game().baseLife1 = _selP1;
    _host->game().baseLife2 = _selP2;
    _host->fireAction(MenuAction::BASE_SELECTOR_COMMIT);
    _host->requestView(0);  // VIEW_TOP
    _hasChanged = false;
    _liftedAt = 0;
  }
  void tick(uint32_t now) override {
    (void)now;
  }

  bool onLift() override {
    if (_hasChanged) _liftedAt = Clock::now();
    return false;
  }
  void idleTick(uint32_t now) override {
    if (!_hasChanged || _liftedAt == 0) return;
    if ((now - _liftedAt) < MENU_AUTO_COMMIT_IDLE_MS) return;
    _hasChanged = false;
    _liftedAt = 0;
    _host->game().baseLife1 = _selP1;
    _host->game().baseLife2 = _selP2;
    _host->fireAction(MenuAction::BASE_SELECTOR_COMMIT);
    _host->requestClose();
  }
  const char* centreActionLabel() const override {
    return "CONFIRM";
  }

private:
  lv_obj_t* _arc1P = nullptr;
  lv_obj_t* _arcP1 = nullptr;
  lv_obj_t* _arcP2 = nullptr;
  lv_obj_t* _ticks1P[VAL_COUNT] = {};
  lv_obj_t* _ticksP1[VAL_COUNT] = {};
  lv_obj_t* _ticksP2[VAL_COUNT] = {};
  int _selP1 = 30;
  int _selP2 = 30;
  int _origP1 = 30;
  int _origP2 = 30;
  bool _hasChanged = false;
  uint32_t _liftedAt = 0;

  lv_obj_t* makeArc(lv_obj_t* parent) {
    lv_obj_t* a = lv_arc_create(parent);
    lv_obj_set_size(a, SCREEN_W, SCREEN_H);
    lv_obj_center(a);
    lv_arc_set_range(a, 0, VAL_COUNT - 1);
    lv_arc_set_angles(a, 0, 0);
    lv_obj_set_style_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_arc_width(a, MENU_RING_THICKNESS, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, MENU_RING_THICKNESS, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, COLOR_MENU_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(a, LV_OBJ_FLAG_HIDDEN);
    return a;
  }

  void buildTicks(lv_obj_t** arr, int count, float rotDeg, float spanDeg) {
    const int R = MENU_OUTER_RADIUS - MENU_RING_THICKNESS / 2;
    for (int i = 0; i < count; ++i) {
      const float frac = (count > 1) ? (float)i / (count - 1) : 0.0f;
      const float angle = normalizeDeg(rotDeg + frac * spanDeg);
      const float rad = angle * PI / 180.0f;
      const int cx = CENTER_X + (int)lroundf(cosf(rad) * R);
      const int cy = CENTER_Y + (int)lroundf(sinf(rad) * R);

      lv_obj_t* t = lv_obj_create(_host->overlay());
      lv_obj_remove_style_all(t);
      lv_obj_set_size(t, TICK_SIZE_NORM, TICK_SIZE_NORM);
      lv_obj_set_pos(t, cx - TICK_SIZE_NORM / 2, cy - TICK_SIZE_NORM / 2);
      lv_obj_set_style_radius(t, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(t, COLOR_MENU_ORANGE, 0);
      lv_obj_set_style_bg_opa(t, LV_OPA_50, 0);
      lv_obj_set_style_border_width(t, 0, 0);
      lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(t, LV_OBJ_FLAG_HIDDEN);
      arr[i] = t;
    }
  }

  void showTicks(lv_obj_t** arr, bool show) {
    for (int i = 0; i < VAL_COUNT; ++i) {
      if (!arr[i]) continue;
      if (show)
        lv_obj_remove_flag(arr[i], LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(arr[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  void hideAll() {
    lv_obj_add_flag(_arc1P, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_arcP1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_arcP2, LV_OBJ_FLAG_HIDDEN);
    showTicks(_ticks1P, false);
    showTicks(_ticksP1, false);
    showTicks(_ticksP2, false);
  }

  void updateArcs() {
    const bool twoP = _host->game().twoPlayer;
    if (!twoP) {
      lv_arc_set_value(_arc1P, _selP1 - VAL_MIN);
    } else {
      lv_arc_set_value(_arcP1, _selP1 - VAL_MIN);
      lv_arc_set_value(_arcP2, _selP2 - VAL_MIN);
    }
  }

  void updateTick(lv_obj_t** arr, float rotDeg, float spanDeg, int selectedVal) {
    const int selIdx = selectedVal - VAL_MIN;
    const int R = MENU_OUTER_RADIUS - MENU_RING_THICKNESS / 2;
    for (int i = 0; i < VAL_COUNT; ++i) {
      if (!arr[i]) continue;
      const bool sel = (i == selIdx);
      const int size = sel ? TICK_SIZE_SEL : TICK_SIZE_NORM;
      const float frac = (VAL_COUNT > 1) ? (float)i / (VAL_COUNT - 1) : 0.0f;
      const float angle = normalizeDeg(rotDeg + frac * spanDeg);
      const float rad = angle * PI / 180.0f;
      const int cx = CENTER_X + (int)lroundf(cosf(rad) * R);
      const int cy = CENTER_Y + (int)lroundf(sinf(rad) * R);
      lv_obj_set_size(arr[i], size, size);
      lv_obj_set_pos(arr[i], cx - size / 2, cy - size / 2);
      lv_obj_set_style_bg_opa(arr[i], sel ? LV_OPA_COVER : LV_OPA_50, 0);
    }
  }

  void updateTicks() {
    const bool twoP = _host->game().twoPlayer;
    if (!twoP) {
      updateTick(_ticks1P, FULL_ROT, FULL_SPAN, _selP1);
    } else {
      updateTick(_ticksP1, P1_ROT, P1_SPAN, _selP1);
      updateTick(_ticksP2, P2_ROT, P2_SPAN, _selP2);
    }
  }

  void renderCentre() {
    CentreLabel& c = _host->centre();
    char buf[16];
    if (_host->game().twoPlayer)
      snprintf(buf, sizeof(buf), "%d | %d", _selP1, _selP2);
    else
      snprintf(buf, sizeof(buf), "%d", _selP1);
    c.set(FA_ICON_BASE_LIFE, UiText::BASE_LIFE, buf);
  }
};

// ==============================================================
// RadialMenu implementation
// ==============================================================

void RadialMenu::begin(lv_obj_t* parent, GameState* state, GameUi* ui, Backlight* backlight, Battery* battery) {
  _gameState = state;
  _gameUi = ui;
  _backlight = backlight;
  _battery = battery;

  buildOverlay(parent);
  _centre.build(_overlay);

  _viewTop = new TopRingView(this);
  _viewChoice = new ChoiceView(this);
  _viewBattery = new BatterySubView(this);
  _viewBrightness = new BrightnessView(this);
  _viewBaseSelector = new BaseSelectorView(this);

  _viewTop->build();
  _viewChoice->build();
  _viewBattery->build();
  _viewBrightness->build();
  _viewBaseSelector->build();

  lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

void RadialMenu::show() {
  _open = true;
  _currentView = VIEW_TOP;
  _fingerDown = true;
  _ignoreOpeningTouch = true;
  _centerTouchActive = false;
  _centerTouchAt = 0;
  _lastFingerSeenAt = Clock::now();
  _pendingAction = MenuAction::NONE;
  _choiceTarget = MenuAction::NONE;

  if (_gameUi) _gameUi->hideForMenu();

  activateView(VIEW_TOP);
  lv_obj_move_foreground(_overlay);
  lv_obj_remove_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

void RadialMenu::close() {
  if (!_open) return;
  currentView()->onExit();
  _open = false;
  _fingerDown = false;
  _centerTouchActive = false;
  _ignoreOpeningTouch = false;
  lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
  if (_gameUi && _gameState) _gameUi->showAfterMenu(_gameState->twoPlayer);
}

void RadialMenu::handleTouch(int x, int y) {
  if (!_open) return;
  const PolarHit hit = polarFromCenter(x, y);

  // The same long-press that opened the menu should not immediately
  // count as a centre tap. Ignore it while the finger remains inside
  // the centre dead-zone; as soon as it leaves, normal hover starts.
  if (_ignoreOpeningTouch && hit.inCentre) return;
  _ignoreOpeningTouch = false;
  _fingerDown = true;
  _lastFingerSeenAt = Clock::now();

  if (hit.inCentre) {
    if (!_centerTouchActive) {
      _centerTouchActive = true;
      _centerTouchAt = Clock::now();
    }
    currentView()->onCentreTouch();
    _centre.setClose(_currentView == VIEW_TOP ? "CLOSE" : currentView()->centreActionLabel());
    return;
  }

  _centerTouchActive = false;
  currentView()->onTouch(x, y, hit);
}

void RadialMenu::markFingerStillDown() {
  if (!_open) return;
  _fingerDown = true;
  _lastFingerSeenAt = Clock::now();
}

RadialMenu::LiftResult RadialMenu::notifyFingerLifted(bool releaseConfirmed) {
  if (!_open || !_fingerDown) return LiftResult::NOTHING;
  if (!releaseConfirmed && !Clock::elapsed(_lastFingerSeenAt, MENU_RELEASE_GRACE_MS)) return LiftResult::NOTHING;

  const bool shouldClose = currentView()->onLift();
  _fingerDown = false;
  _centerTouchActive = false;
  return shouldClose ? LiftResult::CLOSE_MENU : LiftResult::NOTHING;
}

void RadialMenu::tick() {
  if (!_open) return;
  const uint32_t now = Clock::now();

  if (!_fingerDown) {
    currentView()->idleTick(now);
    return;
  }

  // Hold inside centre dead-zone of a sub-view = back out.
  if (_currentView != VIEW_TOP && _centerTouchActive) {
    if (Clock::elapsed(_centerTouchAt, MENU_DWELL_BACK_MS)) {
      activateView(VIEW_TOP);
      _centerTouchActive = true;
      _centerTouchAt = Clock::now();
    }
    return;
  }

  currentView()->tick(now);
}

MenuAction RadialMenu::takePendingAction() {
  const MenuAction a = _pendingAction;
  _pendingAction = MenuAction::NONE;
  return a;
}

void RadialMenu::onGameStateChanged() {
  if (_currentView == VIEW_TOP) {
    static_cast<TopRingView*>(_viewTop)->refreshIcons();
  }
}

void RadialMenu::requestView(int viewId) {
  activateView((ViewId)viewId);
}

void RadialMenu::fireAction(MenuAction a) {
  _pendingAction = a;
}

void RadialMenu::requestClose() {
  close();
}

MenuView* RadialMenu::currentView() {
  switch (_currentView) {
    case VIEW_CHOICE:
      return _viewChoice;
    case VIEW_BATTERY:
      return _viewBattery;
    case VIEW_BRIGHTNESS:
      return _viewBrightness;
    case VIEW_BASE_SELECTOR:
      return _viewBaseSelector;
    case VIEW_TOP:
    default:
      return _viewTop;
  }
}

void RadialMenu::activateView(ViewId v) {
  if (_open && currentView()) currentView()->onExit();
  _currentView = v;
  currentView()->onEnter();
}

void RadialMenu::buildOverlay(lv_obj_t* parent) {
  _overlay = lv_obj_create(parent);
  lv_obj_remove_style_all(_overlay);
  lv_obj_set_size(_overlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(_overlay, 0, 0);
  lv_obj_set_style_bg_color(_overlay, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(_overlay, LV_OPA_100, 0);
  lv_obj_remove_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(_overlay, LV_OBJ_FLAG_CLICKABLE);
}
