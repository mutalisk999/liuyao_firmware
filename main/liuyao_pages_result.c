// main/liuyao_pages_result.c —— 结果页:传统卦盘与分页解读。
#include <stdio.h>
#include <string.h>

#include "bsp_button.h"
#include "esp_log.h"
#include "liuyao_app_internal.h"
#include "liuyao_data.h"
#include "liuyao_theme.h"

static const char *TAG = "liuyao";

// chart_row 取用的字段在排盘失败时全为 NULL/0,直接渲染会出空文本与越界读。
// chart_build/reading_build 在入口统一拦截,页面不应被带无效结果进入。
static bool ensure_valid(struct liyao_app_s *app, ly_state_t fallback) {
    if (app->casting_valid) return true;
    ESP_LOGE(TAG, "结果页收到无效排盘,返回上一页");
    ly_app_goto(app, fallback);
    return false;
}


// ---------------------------------------------------------------------------
// 卦盘页
// ---------------------------------------------------------------------------
static void chart_row(struct liyao_app_s *app, int line_index, int y) {
    const liuyao_line_t *l = &app->result.chart.lines[line_index];
    if (l->moving) {
        // 动爻行朱砂暗晕:让变化一眼可辨。
        lv_obj_t *tint = liuyao_rect_create(app->screen, 6, y - 3, 228, 24,
                                            LY_COLOR_TINT);
        lv_obj_set_style_radius(tint, 4, 0);
    }

    lv_obj_t *spirit = lv_label_create(app->screen);
    lv_obj_set_style_text_font(spirit, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(spirit, lv_color_hex(LY_COLOR_PAPER_DIM), 0);
    lv_label_set_text(spirit, l->spirit);
    lv_obj_set_pos(spirit, 8, y);

    lv_obj_t *najia = lv_label_create(app->screen);
    lv_obj_set_style_text_font(najia, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(najia,
        lv_color_hex(l->moving ? LY_COLOR_CINNABAR : LY_COLOR_PAPER), 0);
    lv_label_set_text_fmt(najia, "%s%s%s", l->relative, l->branch, l->element);
    lv_obj_set_pos(najia, 44, y);

    liuyao_yao_create(app->screen, 116, y + 3, 36, l->yang, l->moving, false);

    if (l->is_shi || l->is_ying) {
        lv_obj_t *mark = lv_label_create(app->screen);
        lv_obj_set_style_text_font(mark, &liuyao_font_16, 0);
        lv_obj_set_style_text_color(mark, lv_color_hex(LY_COLOR_GOLD), 0);
        lv_label_set_text(mark, l->is_shi ? "世" : "应");
        lv_obj_set_pos(mark, 160, y);
    }

    if (l->has_change) {
        liuyao_yao_create(app->screen, 186, y + 3, 36, l->change_yang, false, false);
    } else {
        liuyao_yao_create(app->screen, 186, y + 3, 36, l->yang, false, true);
    }
}

static void chart_build(struct liyao_app_s *app) {
    if (!ensure_valid(app, LY_STATE_METHOD)) return;
    app->screen = liuyao_page_create("卦盘");
    const liuyao_chart_t *c = &app->result.chart;

    // 卦盘整体入框:墨色面板 + 细边,与页面底区分。
    lv_obj_t *panel = liuyao_rect_create(app->screen, 4, 80, 232, 172,
                                         LY_COLOR_PANEL);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(LY_COLOR_PANEL_2), 0);

    lv_obj_t *info = lv_label_create(app->screen);
    lv_obj_set_style_text_font(info, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(info, lv_color_hex(LY_COLOR_GOLD), 0);
    lv_label_set_text_fmt(info, "%s日 %s月 空%s%s", app->day_gz, app->month_gz,
                          liuyao_branch_str(c->void_branches[0]),
                          liuyao_branch_str(c->void_branches[1]));
    lv_obj_set_pos(info, 8, 40);

    lv_obj_t *head_spirit = lv_label_create(app->screen);
    lv_obj_set_style_text_font(head_spirit, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(head_spirit, lv_color_hex(LY_COLOR_PAPER_DIM), 0);
    lv_label_set_text(head_spirit, "六兽");
    lv_obj_set_pos(head_spirit, 8, 64);
    lv_obj_t *head_main = lv_label_create(app->screen);
    lv_obj_set_style_text_font(head_main, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(head_main, lv_color_hex(LY_COLOR_PAPER_DIM), 0);
    lv_label_set_text_fmt(head_main, "本卦 %s", c->name);
    lv_obj_set_pos(head_main, 44, 64);
    lv_obj_t *head_changed = lv_label_create(app->screen);
    lv_obj_set_style_text_font(head_changed, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(head_changed, lv_color_hex(LY_COLOR_PAPER_DIM), 0);
    lv_label_set_text(head_changed, "变卦");
    lv_obj_set_pos(head_changed, 186, 64);

    // 爻行自上而下:上爻在前。
    for (int i = LIUYAO_LINE_COUNT - 1; i >= 0; i--) {
        chart_row(app, i, 88 + (LIUYAO_LINE_COUNT - 1 - i) * 28);
    }

    // 本卦 | 变卦 分界线(置于爻行之上,不被动爻暗晕截断)。
    liuyao_rect_create(app->screen, 178, 92, 1, 150, LY_COLOR_PANEL_2);

    lv_obj_t *footer = lv_label_create(app->screen);
    lv_obj_set_style_text_font(footer, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(footer, lv_color_hex(LY_COLOR_PAPER_DIM), 0);
    lv_label_set_text(footer, "OK 解读 · 长按回封面");
    lv_obj_align(footer, LV_ALIGN_TOP_MID, 0, 268);
}

static void chart_key(struct liyao_app_s *app, bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
        app->reading_page = 0;
        ly_app_goto(app, LY_STATE_READING);
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        ly_app_goto(app, LY_STATE_HOME);
    }
}

// ---------------------------------------------------------------------------
// 解读页:四段文本,上下翻页/滚动。
// ---------------------------------------------------------------------------
static void reading_show(struct liyao_app_s *app) {
    // 同一结果的解读文本不变:只在结果变化时重算,翻页直接复用缓存。
    if (!app->reading_valid) {
        liuyao_compose_reading(&app->result, app->day_gz, app->month_gz,
                               &app->reading_text);
        app->reading_valid = true;
    }
    lv_label_set_text(app->reading.body, app->reading_text.pages[app->reading_page]);
    // 页面超长时 apf 会静默截断(只置 truncated 标记)。在页码上挂一个"*",
    // 让用户知道这一页没装全,而不是误以为原文就到此为止。
    lv_label_set_text_fmt(app->reading.page_label, "%d/%d%s", app->reading_page + 1,
                          app->reading_text.page_count,
                          app->reading_text.truncated ? "*" : "");
    lv_obj_scroll_to(app->reading.scroll, 0, 0, LV_ANIM_OFF);
}

static void reading_build(struct liyao_app_s *app) {
    if (!ensure_valid(app, LY_STATE_CHART)) return;
    app->screen = liuyao_page_create("解读");
    lv_obj_t *scroll = lv_obj_create(app->screen);
    lv_obj_remove_style_all(scroll);
    lv_obj_set_pos(scroll, 8, 42);
    lv_obj_set_size(scroll, 224, 218);
    lv_obj_set_style_clip_corner(scroll, false, 0);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    // 滚动条关闭:滚动条显隐会反复触发无效化,与内容宽度依赖形成重绘循环。
    // 本页用上下键滚动文本,不依赖滚动条提示。
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_OFF);
    // 解读区:墨面板 + 细边圆角,像一页摊开的册子(不透明,避免掩码图层)。
    lv_obj_set_style_bg_color(scroll, lv_color_hex(LY_COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(scroll, 8, 0);
    lv_obj_set_style_border_width(scroll, 1, 0);
    lv_obj_set_style_border_color(scroll, lv_color_hex(LY_COLOR_PANEL_2), 0);
    lv_obj_set_style_pad_all(scroll, 6, 0);
    lv_obj_t *body = lv_label_create(scroll);
    lv_obj_set_style_text_font(body, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(body, lv_color_hex(LY_COLOR_PAPER), 0);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(body, 200);  // 固定宽度:不随滚动条/内容区变化,避免布局反馈循环
    lv_label_set_text(body, "");
    app->reading.scroll = scroll;
    app->reading.body = body;

    lv_obj_t *page_label = lv_label_create(app->screen);
    lv_obj_set_style_text_font(page_label, &liuyao_font_16, 0);
    lv_obj_set_style_text_color(page_label, lv_color_hex(LY_COLOR_GOLD), 0);
    lv_label_set_text(page_label, "");
    lv_obj_set_pos(page_label, 150, 8);  // 电量在右上角,页码左移避开
    app->reading.page_label = page_label;

    liuyao_hint_create(app->screen, 266, "OK 下一页 · 长按回封面");
    app->reading_page = 0;
    reading_show(app);
}

static void reading_key(struct liyao_app_s *app, bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (btn == BSP_BTN_UP && ev == BSP_BTN_CLICK) {
        lv_obj_scroll_by(app->reading.scroll, 0, 40, LV_ANIM_OFF);
    } else if (btn == BSP_BTN_DOWN && ev == BSP_BTN_CLICK) {
        lv_obj_scroll_by(app->reading.scroll, 0, -40, LV_ANIM_OFF);
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
        if (app->reading_page + 1 < app->reading_text.page_count) {
            app->reading_page++;
            reading_show(app);
        } else {
            ly_app_goto(app, LY_STATE_HOME);
        }
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        ly_app_goto(app, LY_STATE_HOME);
    }
}

static const ly_page_ops_t k_chart_ops = {chart_build, chart_key, NULL};
static const ly_page_ops_t k_reading_ops = {reading_build, reading_key, NULL};

const ly_page_ops_t *liuyao_page_ops_result(ly_state_t state) {
    switch (state) {
        case LY_STATE_CHART: return &k_chart_ops;
        case LY_STATE_READING: return &k_reading_ops;
        default: return NULL;
    }
}
