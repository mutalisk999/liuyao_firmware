// main/liuyao_pages_cast.c —— 起卦交互页:自动摇卦(三枚铜钱动画)与手动录爻。
#include <stdio.h>
#include <string.h>

#include "bsp_button.h"
#include "esp_log.h"
#include "esp_random.h"
#include "liuyao_app_internal.h"
#include "liuyao_data.h"
#include "liuyao_theme.h"

#define CAST_ANIM_TICKS 9
#define CAST_ANIM_PERIOD_MS 70
#define CAST_AUTO_NEXT_MS 900

static const char *const TAG = "liuyao-cast";

// 动画帧计数与所属应用(同一时刻至多一段摇卦动画,文件内静态即可;
// LVGL 9 的 lv_timer_t 不透明,不能直接读字段)。
static int s_cast_ticks;
static struct liyao_app_s *s_cast_app;

static void cast_show_progress(struct liyao_app_s *app) {
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        lv_obj_t *slot = app->cast.progress[i];
        lv_obj_clean(slot);
        if (i < app->cast_count) {
            int value = app->casting.lines[i];
            bool yang = value == 7 || value == 9;
            liuyao_yao_create(slot, 0, 6, 24, yang, value == 6 || value == 9, false);
        } else {
            // 未摇之爻:暗色虚位。
            lv_obj_t *empty = lv_obj_create(slot);
            lv_obj_remove_style_all(empty);
            lv_obj_set_size(empty, 24, 7);
            lv_obj_set_pos(empty, 0, 6);
            lv_obj_set_style_border_width(empty, 1, 0);
            lv_obj_set_style_border_color(empty, lv_color_hex(LY_COLOR_PANEL_2), 0);
            lv_obj_set_style_radius(empty, 1, 0);
        }
    }
}

static void cast_set_coin(struct liyao_app_s *app, int index, bool head) {
    lv_obj_set_style_bg_color(app->cast.coins[index],
        lv_color_hex(head ? LY_COLOR_GOLD : LY_COLOR_PANEL_2), 0);
    lv_label_set_text(app->cast.coin_text[index], head ? "正" : "背");
}

static void cast_refresh_header(struct liyao_app_s *app) {
    if (app->cast_count >= LIUYAO_LINE_COUNT) {
        lv_label_set_text(app->cast.title_label, "六爻俱备");
    } else {
        lv_label_set_text_fmt(app->cast.title_label, "第 %d 爻 · %s",
                              app->cast_count + 1,
                              liuyao_line_position_label(app->cast_count + 1));
    }
}

// 六爻摇毕自动进入卦盘。定时器回调运行于 LVGL 任务,可直接删屏。
static void cast_auto_next(lv_timer_t *timer);

static void cast_anim_stop(struct liyao_app_s *app, bool settle) {
    if (app->anim_timer) {
        lv_timer_del(app->anim_timer);
        app->anim_timer = NULL;
    }
    app->cast_busy = false;
    if (!settle) return;
    // 三枚铜钱:正面记 3,背面记 2,合计 6..9。
    uint32_t rand = esp_random();
    int value = 0;
    for (int i = 0; i < 3; i++) {
        bool head = ((rand >> (i * 5)) & 1) != 0;
        cast_set_coin(app, i, head);
        value += head ? 3 : 2;
    }
    app->casting.lines[app->cast_count++] = value;
    lv_label_set_text_fmt(app->cast.result_label, "%s %d%s", value == 9 ? "老阳"
                          : value == 8 ? "少阴"
                          : value == 7 ? "少阳" : "老阴", value,
                          (value == 6 || value == 9) ? " · 动" : "");
    cast_show_progress(app);
    cast_refresh_header(app);
    if (app->cast_count >= LIUYAO_LINE_COUNT) {
        lv_timer_t *t = lv_timer_create(cast_auto_next, CAST_AUTO_NEXT_MS, app);
        // 内存不足时定时器建不出来:直接当场完成切页,避免卡在"六爻俱备"。
        if (t == NULL) {
            ESP_LOGE(TAG, "自动切页定时器创建失败,直接进入卦盘");
            if (!ly_app_cast_lines_to_result(app)) {
                cast_failed(app);
            } else {
                ly_app_goto(app, LY_STATE_CHART);
            }
            ly_app_notify(app);
        } else {
            app->auto_timer = t;
        }
    }
}

// 六爻摇毕自动进入卦盘。定时器回调运行于 LVGL 任务,可直接删屏。
static void cast_auto_next(lv_timer_t *timer) {
    struct liyao_app_s *app = s_cast_app;
    (void)timer;
    lv_timer_del(timer);
    app->auto_timer = NULL;
    // 排盘失败时不进入卦盘页:chart 全零会导致 NULL 文本与越界读。
    if (!ly_app_cast_lines_to_result(app)) {
        cast_failed(app);
    } else {
        ly_app_goto(app, LY_STATE_CHART);
    }
    ly_app_notify(app);  // 立即唤醒应用任务完成切页
}

static void cast_anim_tick(lv_timer_t *timer) {
    struct liyao_app_s *app = s_cast_app;
    (void)timer;
    s_cast_ticks++;
    uint32_t rand = esp_random();
    for (int i = 0; i < 3; i++) {
        cast_set_coin(app, i, ((rand >> (i * 7)) & 1) != 0);
    }
    if (s_cast_ticks >= CAST_ANIM_TICKS) {
        cast_anim_stop(app, true);
    }
}

static void cast_build(struct liyao_app_s *app) {
    app->screen = liuyao_page_create("摇卦");
    app->cast_count = 0;
    app->cast_busy = false;
    app->anim_timer = NULL;
    app->auto_timer = NULL;
    s_cast_app = app;
    memset(&app->casting.lines, 0, sizeof(app->casting.lines));

    app->cast.title_label = lv_label_create(app->screen);
    lv_obj_set_style_text_font(app->cast.title_label, &liuyao_font_24, 0);
    lv_obj_set_style_text_color(app->cast.title_label, lv_color_hex(LY_COLOR_GOLD), 0);
    lv_obj_set_pos(app->cast.title_label, 60, 46);

    for (int i = 0; i < 3; i++) {
        lv_obj_t *coin = lv_obj_create(app->screen);
        lv_obj_remove_style_all(coin);
        lv_obj_set_size(coin, 52, 52);
        lv_obj_set_pos(coin, 24 + i * 60, 92);
        lv_obj_set_style_radius(coin, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(coin, lv_color_hex(LY_COLOR_PANEL_2), 0);
        lv_obj_set_style_bg_opa(coin, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(coin, 2, 0);
        lv_obj_set_style_border_color(coin, lv_color_hex(LY_COLOR_GOLD), 0);
        lv_obj_t *face = lv_label_create(coin);  // 保持为 child 0:cast_set_coin 依赖
        lv_obj_set_style_text_font(face, &liuyao_font_24, 0);
        lv_obj_set_style_text_color(face, lv_color_hex(LY_COLOR_PAPER), 0);
        lv_obj_center(face);
        lv_label_set_text(face, "?");
        // 内环:双圈铜钱的金属质感(仅描边,不遮字)。
        lv_obj_t *inner = lv_obj_create(coin);
        lv_obj_remove_style_all(inner);
        lv_obj_set_size(inner, 40, 40);
        lv_obj_center(inner);
        lv_obj_set_style_radius(inner, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(inner, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(inner, 1, 0);
        lv_obj_set_style_border_color(inner, lv_color_hex(LY_COLOR_GOLD), 0);
        lv_obj_set_style_border_opa(inner, LV_OPA_50, 0);
        app->cast.coins[i] = coin;
        app->cast.coin_text[i] = face;
    }

    app->cast.result_label = lv_label_create(app->screen);
    lv_obj_set_style_text_font(app->cast.result_label, &liuyao_font_24, 0);
    lv_obj_set_style_text_color(app->cast.result_label,
                                lv_color_hex(LY_COLOR_PAPER), 0);
    lv_obj_align(app->cast.result_label, LV_ALIGN_TOP_MID, 0, 166);
    lv_label_set_text(app->cast.result_label, "");  // 清掉 LVGL 默认的 "Text"

    // 右侧六爻进度(上爻在上,初爻在下)收进面板,与铜钱区形成分区。
    // 面板 x>=198:给第三枚铜钱(右缘 196)留出间隙,避免遮挡。
    lv_obj_t *progress_panel = liuyao_rect_create(app->screen, 198, 84, 40, 154,
                                                  LY_COLOR_PANEL);
    lv_obj_set_style_border_width(progress_panel, 1, 0);
    lv_obj_set_style_border_color(progress_panel, lv_color_hex(LY_COLOR_PANEL_2), 0);
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        lv_obj_t *slot = lv_obj_create(app->screen);
        lv_obj_remove_style_all(slot);
        lv_obj_set_size(slot, 26, 20);
        lv_obj_set_pos(slot, 201, 92 + (LIUYAO_LINE_COUNT - 1 - i) * 24);
        app->cast.progress[i] = slot;
    }

    app->cast.hint_label = liuyao_hint_create(app->screen, 264,
                                              "OK 摇卦 · 长按返回");

    cast_show_progress(app);
    cast_refresh_header(app);
}

static void cast_key(struct liyao_app_s *app, bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
        if (app->cast_busy || app->cast_count >= LIUYAO_LINE_COUNT) return;
        app->cast_busy = true;
        lv_label_set_text(app->cast.result_label, "");
        lv_timer_t *t = lv_timer_create(cast_anim_tick, CAST_ANIM_PERIOD_MS, app);
        // 建不出定时器就不能摇卦:解除忙态,让用户可以长按返回或重试。
        if (t == NULL) {
            ESP_LOGE(TAG, "摇卦动画定时器创建失败");
            app->cast_busy = false;
            return;
        }
        app->anim_timer = t;
        s_cast_ticks = 0;
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        ly_app_goto(app, LY_STATE_HOME);
    }
}

static void cast_exit(struct liyao_app_s *app) {
    if (app->anim_timer) {
        lv_timer_del(app->anim_timer);
        app->anim_timer = NULL;
    }
    if (app->auto_timer) {
        lv_timer_del(app->auto_timer);
        app->auto_timer = NULL;
    }
    app->cast_busy = false;
    s_cast_app = NULL;
}

// ---------------------------------------------------------------------------
// 手动录爻:逐爻选择 少阳/少阴/老阳/老阴。
// ---------------------------------------------------------------------------
static const int k_manual_values[LY_MANUAL_OPTION_COUNT] = {7, 8, 9, 6};
static const char *const k_manual_labels[LY_MANUAL_OPTION_COUNT] = {
    "少阳 7", "少阴 8", "老阳 9 动", "老阴 6 动",
};

static void manual_refresh(struct liyao_app_s *app) {
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        int option = -1;
        for (int k = 0; k < LY_MANUAL_OPTION_COUNT; k++) {
            if (k_manual_values[k] == app->casting.lines[i]) option = k;
        }
        // rows 自上而下为 上爻..初爻,与卦盘页一致(爻位序号仍按 i+1)。
        lv_obj_t *row = app->manual.rows[LIUYAO_LINE_COUNT - 1 - i];
        lv_obj_t *label = lv_obj_get_child(row, 0);
        if (option < 0) {
            lv_label_set_text_fmt(label, "%s  —", liuyao_line_position_label(i + 1));
        } else {
            lv_label_set_text_fmt(label, "%s  %s", liuyao_line_position_label(i + 1),
                                  k_manual_labels[option]);
        }
        bool selected = i == app->manual_row;
        lv_obj_set_style_bg_color(row,
            lv_color_hex(selected ? LY_COLOR_PANEL_2 : LY_COLOR_PANEL), 0);
        lv_obj_set_style_border_color(row,
            lv_color_hex(selected ? LY_COLOR_CINNABAR : LY_COLOR_PANEL_2), 0);
        lv_obj_set_style_text_color(label,
            lv_color_hex(selected ? LY_COLOR_PAPER : LY_COLOR_PAPER_DIM), 0);
    }
}

static void manual_build(struct liyao_app_s *app) {
    app->screen = liuyao_page_create("手动录爻");
    app->manual_row = 0;
    memset(app->casting.lines, 0, sizeof(app->casting.lines));
    // 行自上而下为 上爻..初爻,与卦盘页和摇卦进度条方向一致,
    // 用户对照纸质卦盘录入时不会读反。
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(app->screen);
        lv_obj_remove_style_all(row);
        lv_obj_set_pos(row, 20, 46 + i * 33);
        lv_obj_set_size(row, 200, 28);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(LY_COLOR_PANEL), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(LY_COLOR_PANEL_2), 0);
        lv_obj_t *label = lv_label_create(row);
        lv_obj_set_style_text_font(label, &liuyao_font_16, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(LY_COLOR_PAPER), 0);
        lv_obj_set_pos(label, 10, 4);
        lv_label_set_text(label, "");
        app->manual.rows[i] = row;
    }
    liuyao_hint_create(app->screen, 258, "上下换值 · OK 下爻");
    manual_refresh(app);
}

// 未选爻(值为 0)时,UP/DOWN 从哪个选项开始转。
// 表里 UP 走 少阳->少阴->老阳->老阴,DOWN 反向;两者第一次按下都落在
// 与 OK 默认值(少阳)相邻的选项上,避免三个按键给出三种不同起点。
static int manual_option_of(int value) {
    for (int k = 0; k < LY_MANUAL_OPTION_COUNT; k++) {
        if (k_manual_values[k] == value) return k;
    }
    return -1;  // 未选值
}

static void manual_step(struct liyao_app_s *app, int delta) {
    int option = manual_option_of(app->casting.lines[app->manual_row]);
    if (option < 0) {
        // 未选值:按"从少阳出发"对齐 OK 的默认行为。
        option = delta > 0 ? 0 : 1;
    } else {
        option = (option + delta + LY_MANUAL_OPTION_COUNT) % LY_MANUAL_OPTION_COUNT;
    }
    app->casting.lines[app->manual_row] = k_manual_values[option];
    manual_refresh(app);
}

static void manual_key(struct liyao_app_s *app, bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (btn == BSP_BTN_UP && ev == BSP_BTN_CLICK) {
        manual_step(app, 1);
    } else if (btn == BSP_BTN_DOWN && ev == BSP_BTN_CLICK) {
        manual_step(app, -1);
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
        if (app->casting.lines[app->manual_row] == 0) {
            // 未选值:填入少阳,避免跳爻。
            app->casting.lines[app->manual_row] = k_manual_values[0];
        }
        if (app->manual_row >= LIUYAO_LINE_COUNT - 1) {
            app->cast_count = LIUYAO_LINE_COUNT;
            // 排盘失败不进入卦盘页(同 cast_auto_next 的处理)。
            if (!ly_app_cast_lines_to_result(app)) {
                cast_failed(app);
            } else {
                ly_app_goto(app, LY_STATE_CHART);
            }
        } else {
            app->manual_row++;
            manual_refresh(app);
        }
    } else if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        ly_app_goto(app, LY_STATE_METHOD);
    }
}

static const ly_page_ops_t k_cast_ops = {cast_build, cast_key, cast_exit};
static const ly_page_ops_t k_manual_ops = {manual_build, manual_key, NULL};

const ly_page_ops_t *liuyao_page_ops_cast(ly_state_t state) {
    switch (state) {
        case LY_STATE_CASTING: return &k_cast_ops;
        case LY_STATE_MANUAL: return &k_manual_ops;
        default: return NULL;
    }
}
