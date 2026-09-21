// main/liuyao_theme.c —— 六爻应用视觉主题实现。
#include "liuyao_theme.h"

#include <stdio.h>

static lv_obj_t *rect(lv_obj_t *parent, int x, int y, int w, int h,
                      uint32_t color) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 1, 0);
    return obj;
}

lv_obj_t *liuyao_page_create(const char *title) {
    lv_obj_t *root = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(root, lv_color_hex(LY_COLOR_BG), 0);

    if (title) {
        lv_obj_t *label = lv_label_create(root);
        lv_obj_set_style_text_font(label, &liuyao_font_24, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(LY_COLOR_PAPER), 0);
        lv_label_set_text(label, title);
        lv_obj_set_pos(label, 12, 6);
    }
    return root;
}

void liuyao_battery_update(lv_obj_t *battery_label, int soc) {
    if (!battery_label) return;
    if (soc < 0) {
        lv_label_set_text(battery_label, "--");
    } else {
        lv_label_set_text_fmt(battery_label, "%d%%", soc);
    }
    lv_obj_set_style_text_color(battery_label,
                                soc >= 0 && soc < 20
                                    ? lv_color_hex(LY_COLOR_CINNABAR)
                                    : lv_color_hex(LY_COLOR_PAPER_DIM),
                                0);
}

lv_obj_t *liuyao_yao_create(lv_obj_t *parent, int x, int y, int width,
                            bool yang, bool moving, bool dim) {
    uint32_t color = dim ? LY_COLOR_YIN : LY_COLOR_YANG;
    int bar_h = width > 40 ? 10 : 7;
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, width + 10, bar_h);
    if (yang) {
        rect(box, 0, 0, width, bar_h, color);
    } else {
        int seg = (width - 4) / 2;
        rect(box, 0, 0, seg, bar_h, color);
        rect(box, seg + 4, 0, seg, bar_h, color);
    }
    if (moving) {
        // 动爻标记:爻右一个朱砂点,老阳圆点、老阴方点(图形化,不依赖字形)。
        lv_obj_t *dot = rect(box, width + 4, bar_h / 2 - 2, 5, 5, LY_COLOR_CINNABAR);
        lv_obj_set_style_radius(dot, yang ? LV_RADIUS_CIRCLE : 1, 0);
    }
    return box;
}

static lv_obj_t *circle(lv_obj_t *parent, int cx, int cy, int r, uint32_t color) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, r * 2, r * 2);
    lv_obj_set_pos(obj, cx - r, cy - r);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

lv_obj_t *liuyao_taiji_create(lv_obj_t *parent, int cx, int cy, int radius) {
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    int d = radius * 2;
    lv_obj_set_pos(root, cx - radius, cy - radius);
    lv_obj_set_size(root, d, d);

    // 外盘金环 + 阳鱼白底盘。
    circle(root, radius, radius, radius, LY_COLOR_GOLD);
    circle(root, radius, radius, radius - 2, LY_COLOR_PAPER);

    // 阴鱼:下半圆(弧宽=半径 → 实心半圆)。
    lv_obj_t *half = lv_arc_create(root);
    lv_obj_remove_style_all(half);
    lv_obj_set_size(half, d - 4, d - 4);
    lv_obj_set_pos(half, 2, 2);
    lv_arc_set_rotation(half, 180);
    lv_arc_set_bg_angles(half, 0, 180);
    lv_arc_set_value(half, 100);
    lv_obj_set_style_arc_width(half, radius - 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(half, lv_color_hex(LY_COLOR_PANEL_2), LV_PART_INDICATOR);
    lv_obj_remove_style(half, NULL, LV_PART_KNOB);

    // 阴阳双首小圆:上首为阴(深),下首为阳(浅),构成 S 形分界。
    circle(root, radius, radius / 2, radius / 2 - 1, LY_COLOR_PANEL_2);
    circle(root, radius, radius * 3 / 2, radius / 2 - 1, LY_COLOR_PAPER);
    // 鱼眼。
    circle(root, radius, radius / 2, (radius / 2 - 1) / 2, LY_COLOR_PAPER);
    circle(root, radius, radius * 3 / 2, (radius / 2 - 1) / 2, LY_COLOR_PANEL_2);
    return root;
}
