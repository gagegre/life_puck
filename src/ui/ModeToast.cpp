// ModeToast.cpp
//
// Method bodies for ModeToast.

#include "ModeToast.h"
#include "Clock.h"

void ModeToast::begin(lv_obj_t* parent) {
  _parent = parent;

  // ---- full-screen heavy dim ----
  // Almost-black overlay: anything behind the toast (life counter,
  // sub-labels, delta badge, battery) is virtually invisible while
  // the toast is up. This commits to the modal feel.
  _dim = lv_obj_create(parent);
  lv_obj_remove_style_all(_dim);
  lv_obj_set_size(_dim, SCREEN_W, SCREEN_H);
  lv_obj_center(_dim);
  lv_obj_set_style_bg_color(_dim, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(_dim, 220, 0);  // ~86%
  lv_obj_remove_flag(_dim, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(_dim, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(_dim, LV_OBJ_FLAG_HIDDEN);

  // ---- coloured backdrop disc ----
  _circle = lv_obj_create(parent);
  lv_obj_remove_style_all(_circle);
  lv_obj_set_size(_circle, CIRCLE_DIAM, CIRCLE_DIAM);
  lv_obj_center(_circle);
  lv_obj_set_style_radius(_circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(_circle, COLOR_FG, 0);
  lv_obj_set_style_bg_opa(_circle, 50, 0);  // soft tint, not solid
  lv_obj_set_style_border_width(_circle, 2, 0);
  lv_obj_set_style_border_color(_circle, COLOR_FG, 0);
  lv_obj_set_style_border_opa(_circle, LV_OPA_COVER, 0);
  lv_obj_remove_flag(_circle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(_circle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(_circle, LV_OBJ_FLAG_HIDDEN);

  // ---- icon (upper half of circle) ----
  _icon = lv_label_create(parent);
  lv_obj_set_style_text_font(_icon, &font_awesome_icons, 0);
  lv_obj_set_style_text_color(_icon, COLOR_FG, 0);
  lv_obj_set_style_text_opa(_icon, LV_OPA_COVER, 0);
  lv_obj_align(_icon, LV_ALIGN_CENTER, 0, ICON_OFFSET_Y);
  lv_obj_add_flag(_icon, LV_OBJ_FLAG_HIDDEN);

  // ---- title (lower half of circle) ----
  // Width-capped + wrap mode so longer captions like
  // "BRIGHTNESS 75%" fold to two lines and still fit inside the disc.
  _title = lv_label_create(parent);
  lv_obj_set_width(_title, TITLE_MAX_W);
  lv_label_set_long_mode(_title, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(_title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(_title, COLOR_FG, 0);
  lv_obj_set_style_text_opa(_title, LV_OPA_COVER, 0);
  lv_obj_align(_title, LV_ALIGN_CENTER, 0, TITLE_OFFSET_Y);
  lv_obj_add_flag(_title, LV_OBJ_FLAG_HIDDEN);
}

void ModeToast::show(const char* icon, const char* title, const char* value, lv_color_t color, bool coverScreen) {
  if (!_dim) return;

  char line[48];
  line[0] = '\0';
  if (title && title[0] && value && value[0]) {
    snprintf(line, sizeof(line), "%s %s", title, value);
  } else if (title && title[0]) {
    snprintf(line, sizeof(line), "%s", title);
  } else if (value && value[0]) {
    snprintf(line, sizeof(line), "%s", value);
  }

  // Re-stamp font/opacity each call: lv_obj_set_style_text_color() can
  // flush the local style cache and revert the font on the next render.
  lv_obj_set_style_text_font(_icon, &font_awesome_icons, 0);
  lv_obj_set_style_text_opa(_icon, LV_OPA_COVER, 0);
  lv_label_set_text(_icon, icon ? icon : "");
  lv_label_set_text(_title, line);

  // Normal toasts are strongly dimmed but still translucent. Sleep uses
  // the same toast machinery with a fully opaque backdrop, so the life
  // counter is completely hidden while the ZZZ confirmation is shown.
  lv_obj_set_style_bg_color(_dim, coverScreen ? lv_color_hex(0x020611) : lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(_dim, coverScreen ? LV_OPA_COVER : 220, 0);

  // Icon + circle outline take the accent colour. Title stays white.
  lv_obj_set_style_text_color(_icon, color, 0);
  lv_obj_set_style_text_color(_title, COLOR_FG, 0);
  lv_obj_set_style_bg_color(_circle, color, 0);
  lv_obj_set_style_border_color(_circle, color, 0);

  _shownAt = Clock::now();
  _visible = true;

  // Reveal in z-order: dim -> circle -> icon -> title.
  lv_obj_remove_flag(_dim, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_circle, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_icon, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(_title, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(_dim);
  lv_obj_move_foreground(_circle);
  lv_obj_move_foreground(_icon);
  lv_obj_move_foreground(_title);
}

void ModeToast::update() {
  if (!_visible || !_dim) return;
  if (Clock::elapsed(_shownAt, TOAST_MS)) {
    _visible = false;
    lv_obj_add_flag(_dim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_circle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_title, LV_OBJ_FLAG_HIDDEN);
  }
}
