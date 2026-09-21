// main/liuyao_pages_flow.c —— 六爻应用流程页:封面/问事分类/视角/日期/起卦方式。
// 全新设计的竖屏 UI:墨底宣纸字,三键导航(上下选择,OK 确认,长按返回)。
#include <stdio.h>
#include <string.h>

#include "bsp_button.h"
#include "liuyao_app_internal.h"
#include "liuyao_data.h"
#include "liuyao_theme.h"

// ---------------------------------------------------------------------------
// 封面
// ---------------------------------------------------------------------------
static void home_build(struct liyao_app_s *app) {
    app->screen = liuyao_page_create(NULL);
    liuyao_taiji_create(app->screen, 120, 108, 52);
    lv_obj_t *title = lv_label_create(app->screen);
    lv_obj_set_style_text_font(title, &liuyao_font_48, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(LY_COLOR_PAPER), 0);
    lv_label_set_text(title, "六爻");
    lv_obj_set_pos(title, 78, 176);
    lv_obj_t *subtitle = lv_label_create(app->screen);
    lv_obj_set_style_text_font(subtitle, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(LY_COLOR_PAPER_DIM), 0);
    lv_label_set_text(subtitle, "文王纳甲 · 铜钱摇卦");
    lv_obj_set_pos(subtitle, 58, 234);
    lv_obj_t *hint = lv_label_create(app->screen);
    lv_obj_set_style_text_font(hint, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(LY_COLOR_GOLD), 0);
    lv_label_set_text(hint, "按 OK 开始起卦");
    lv_obj_set_pos(hint, 66, 272);
}

static void home_key(struct liyao_app_s *app, bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
        ly_app_goto(app, LY_STATE_CATEGORY);
    }
}

// ---------------------------------------------------------------------------
// 问事分类(3×3 网格)
// ---------------------------------------------------------------------------
static void grid_refresh(struct liyao_app_s *app, const ly_grid_widgets_t *grid,
                         int count) {
    for (int i = 0; i < count; i++) {
        ly_style_option(grid->cells[i], i == app->sel);
    }
}

static void category_build(struct liyao_app_s *app) {
    app->screen = liuyao_page_create("问何事");
    for (int i = 0; i < LIUYAO_CAT_COUNT; i++) {
        lv_obj_t *panel = lv_obj_create(app->screen);
        lv_obj_remove_style_all(panel);
        int col = i % 3;
        int row = i / 3;
        lv_obj_set_pos(panel, 12 + col * 74, 44 + row * 62);
        lv_obj_set_size(panel, 70, 54);
        lv_obj_t *label = lv_label_create(panel);
        lv_obj_set_style_text_font(label, &liuyao_font_24, 0);
        lv_obj_center(label);
        lv_label_set_text(label, liuyao_category_label(i));
        app->category.cells[i] = panel;
    }
    lv_obj_t *hint = lv_label_create(app->screen);
    lv_obj_set_style_text_font(hint, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(LY_COLOR_PAPER_DIM), 0);
    lv_label_set_text(hint, "上下选择 · OK 确认");
    lv_obj_set_pos(hint, 44, 236);
    app->sel = 0;
    grid_refresh(app, &app->category, LIUYAO_CAT_COUNT);
}

static void category_key(struct liyao_app_s *app, bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (btn == BSP_BTN_UP && ev == BSP_BTN_CLICK) {
        app->sel = (app->sel + LIUYAO_CAT_COUNT - 1) % LIUYAO_CAT_COUNT;
        grid_refresh(app, &app->category, LIUYAO_CAT_COUNT);
    } else if (btn == BSP_BTN_DOWN && ev == BSP_BTN_CLICK) {
        app->sel = (app->sel + 1) % LIUYAO_CAT_COUNT;
        grid_refresh(app, &app->category, LIUYAO_CAT_COUNT);
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
        app->casting.category = (liuyao_category_t)app->sel;
        ly_app_goto(app, app->casting.category == LIUYAO_CAT_RELATION
                             ? LY_STATE_PERSPECTIVE
                             : LY_STATE_DATE);
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        ly_app_goto(app, LY_STATE_HOME);
    }
}

// ---------------------------------------------------------------------------
// 感情视角(3 项纵向列表)
// ---------------------------------------------------------------------------
static void perspective_build(struct liyao_app_s *app) {
    app->screen = liuyao_page_create("感情视角");
    const int count = LIUYAO_PERSP_FEMALE + 1;
    for (int i = 0; i < count; i++) {
        lv_obj_t *panel = lv_obj_create(app->screen);
        lv_obj_remove_style_all(panel);
        lv_obj_set_pos(panel, 30, 52 + i * 52);
        lv_obj_set_size(panel, 180, 44);
        lv_obj_t *label = lv_label_create(panel);
        lv_obj_set_style_text_font(label, &liuyao_font_24, 0);
        lv_obj_center(label);
        lv_label_set_text(label, liuyao_perspective_label(i));
        app->perspective.cells[i] = panel;
    }
    lv_obj_t *hint = lv_label_create(app->screen);
    lv_obj_set_style_text_font(hint, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(LY_COLOR_PAPER_DIM), 0);
    lv_label_set_text(hint, "男问妻财 · 女问官鬼");
    lv_obj_set_pos(hint, 44, 226);
    app->sel = LIUYAO_PERSP_UNSPECIFIED;
    grid_refresh(app, &app->perspective, LIUYAO_PERSP_FEMALE + 1);
}

static void perspective_key(struct liyao_app_s *app, bsp_btn_t btn,
                            bsp_btn_ev_t ev) {
    const int count = LIUYAO_PERSP_FEMALE + 1;
    if (btn == BSP_BTN_UP && ev == BSP_BTN_CLICK) {
        app->sel = (app->sel + count - 1) % count;
        grid_refresh(app, &app->perspective, count);
    } else if (btn == BSP_BTN_DOWN && ev == BSP_BTN_CLICK) {
        app->sel = (app->sel + 1) % count;
        grid_refresh(app, &app->perspective, count);
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
        app->casting.perspective = (liuyao_perspective_t)app->sel;
        ly_app_goto(app, LY_STATE_DATE);
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        ly_app_goto(app, LY_STATE_CATEGORY);
    }
}

// ---------------------------------------------------------------------------
// 起卦日期(年/月/日/时 四字段)
// ---------------------------------------------------------------------------
static const int k_date_min[LY_DATE_FIELD_COUNT] = {2020, 1, 1, 0};
static const int k_date_max[LY_DATE_FIELD_COUNT] = {2040, 12, 31, 23};
static const char *const k_date_title[LY_DATE_FIELD_COUNT] = {"年", "月", "日", "时"};

static void date_refresh(struct liyao_app_s *app) {
    int values[LY_DATE_FIELD_COUNT] = {app->year, app->month, app->day, app->hour};
    for (int i = 0; i < LY_DATE_FIELD_COUNT; i++) {
        lv_label_set_text_fmt(app->date.values[i], "%d", values[i]);
        lv_obj_set_style_text_color(app->date.values[i],
            lv_color_hex(i == app->date_field ? LY_COLOR_GOLD : LY_COLOR_PAPER), 0);
        lv_obj_set_style_text_color(app->date.titles[i],
            lv_color_hex(i == app->date_field ? LY_COLOR_GOLD : LY_COLOR_PAPER_DIM), 0);
    }
}

static void date_clamp(struct liyao_app_s *app) {
    if (app->month < 1) app->month = 12;
    if (app->month > 12) app->month = 1;
    int dim = ly_days_in_month_clamped(app->year, app->month, app->day);
    if (app->day > dim) app->day = dim;
    if (app->day < 1) app->day = 1;
    if (app->hour < 0) app->hour = 23;
    if (app->hour > 23) app->hour = 0;
    if (app->year < k_date_min[0]) app->year = k_date_max[0];
    if (app->year > k_date_max[0]) app->year = k_date_min[0];
}

static void date_build(struct liyao_app_s *app) {
    app->screen = liuyao_page_create("起卦时间");
    // 字段横坐标按内容宽度分配(年 4 位数字最宽),避免相邻字段粘连。
    static const int k_field_x[LY_DATE_FIELD_COUNT] = {10, 82, 132, 182};
    for (int i = 0; i < LY_DATE_FIELD_COUNT; i++) {
        lv_obj_t *title = lv_label_create(app->screen);
        lv_obj_set_style_text_font(title, &liuyao_font_24, 0);
        lv_obj_set_pos(title, k_field_x[i], 108);
        lv_label_set_text(title, k_date_title[i]);
        app->date.titles[i] = title;
        lv_obj_t *value = lv_label_create(app->screen);
        lv_obj_set_style_text_font(value, &liuyao_font_24, 0);
        lv_obj_set_pos(value, k_field_x[i], 140);
        app->date.values[i] = value;
    }
    lv_obj_t *hint = lv_label_create(app->screen);
    lv_obj_set_style_text_font(hint, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(LY_COLOR_PAPER_DIM), 0);
    lv_label_set_text(hint, "上下调整 · OK 下一项");
    lv_obj_set_pos(hint, 40, 212);
    app->date_field = 0;
    date_refresh(app);
}

static void date_key(struct liyao_app_s *app, bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (btn == BSP_BTN_UP && ev == BSP_BTN_CLICK) {
        int *fields[LY_DATE_FIELD_COUNT] = {&app->year, &app->month, &app->day,
                                            &app->hour};
        *fields[app->date_field] += 1;
        date_clamp(app);
        date_refresh(app);
    } else if (btn == BSP_BTN_DOWN && ev == BSP_BTN_CLICK) {
        int *fields[LY_DATE_FIELD_COUNT] = {&app->year, &app->month, &app->day,
                                            &app->hour};
        *fields[app->date_field] -= 1;
        date_clamp(app);
        date_refresh(app);
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
        app->date_field++;
        if (app->date_field >= LY_DATE_FIELD_COUNT) {
            app->date_field = 0;
            ly_app_save_last_date(app);
            ly_app_goto(app, LY_STATE_METHOD);
        } else {
            date_refresh(app);
        }
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        ly_app_goto(app, LY_STATE_CATEGORY);
    }
}

// ---------------------------------------------------------------------------
// 起卦方式(2 项)
// ---------------------------------------------------------------------------
static void method_build(struct liyao_app_s *app) {
    app->screen = liuyao_page_create("起卦方式");
    static const char *const k_options[2] = {"摇卦(自动)", "手动录爻"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *panel = lv_obj_create(app->screen);
        lv_obj_remove_style_all(panel);
        lv_obj_set_pos(panel, 30, 84 + i * 56);
        lv_obj_set_size(panel, 180, 46);
        lv_obj_t *label = lv_label_create(panel);
        lv_obj_set_style_text_font(label, &liuyao_font_24, 0);
        lv_obj_center(label);
        lv_label_set_text(label, k_options[i]);
        app->perspective.cells[i] = panel;  // 复用通用网格部件
    }
    lv_obj_t *hint = lv_label_create(app->screen);
    lv_obj_set_style_text_font(hint, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(LY_COLOR_PAPER_DIM), 0);
    lv_label_set_text(hint, "已有铜钱结果请选手动录爻");
    lv_obj_set_pos(hint, 28, 212);
    app->sel = 0;
    grid_refresh(app, &app->perspective, 2);
}

static void method_key(struct liyao_app_s *app, bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (btn == BSP_BTN_UP && ev == BSP_BTN_CLICK) {
        app->sel = (app->sel + 1) % 2;
        grid_refresh(app, &app->perspective, 2);
    } else if (btn == BSP_BTN_DOWN && ev == BSP_BTN_CLICK) {
        app->sel = (app->sel + 1) % 2;
        grid_refresh(app, &app->perspective, 2);
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
        ly_app_goto(app, app->sel == 0 ? LY_STATE_CASTING : LY_STATE_MANUAL);
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        ly_app_goto(app, LY_STATE_DATE);
    }
}

static const ly_page_ops_t k_home_ops = {home_build, home_key, NULL};
static const ly_page_ops_t k_category_ops = {category_build, category_key, NULL};
static const ly_page_ops_t k_perspective_ops = {perspective_build, perspective_key,
                                                NULL};
static const ly_page_ops_t k_date_ops = {date_build, date_key, NULL};
static const ly_page_ops_t k_method_ops = {method_build, method_key, NULL};

const ly_page_ops_t *liuyao_page_ops_flow(ly_state_t state) {
    switch (state) {
        case LY_STATE_HOME: return &k_home_ops;
        case LY_STATE_CATEGORY: return &k_category_ops;
        case LY_STATE_PERSPECTIVE: return &k_perspective_ops;
        case LY_STATE_DATE: return &k_date_ops;
        case LY_STATE_METHOD: return &k_method_ops;
        default: return NULL;
    }
}
