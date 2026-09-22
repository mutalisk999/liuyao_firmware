// main/liuyao_app_internal.h —— 六爻应用内部共享结构(仅 liuyao_app*.c 使用)。
#pragma once

#include "bsp_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "liuyao_engine.h"
#include "liuyao_reading.h"
#include "lvgl.h"

// 应用页面状态(自设计的导航流,不复用 baseline demo 菜单)。
typedef enum {
    LY_STATE_HOME = 0,     // 封面
    LY_STATE_CATEGORY,     // 问事分类
    LY_STATE_PERSPECTIVE,  // 感情视角(仅感情类)
    LY_STATE_DATE,         // 起卦日期时间
    LY_STATE_METHOD,       // 起卦方式
    LY_STATE_CASTING,      // 摇卦
    LY_STATE_MANUAL,       // 手动录爻
    LY_STATE_CHART,        // 卦盘
    LY_STATE_READING,      // 解读
    LY_STATE_COUNT,
} ly_state_t;

#define LY_DATE_FIELD_COUNT 4  // 年 月 日 时
#define LY_MANUAL_OPTION_COUNT 4

// 摇卦动画部件。
typedef struct {
    lv_obj_t *coins[3];      // 三枚铜钱(圆)
    lv_obj_t *coin_text[3];  // 正/背
    lv_obj_t *result_label;  // 本爻结果
    lv_obj_t *progress[6];   // 已成之爻(小爻条容器)
    lv_obj_t *title_label;   // 第 N 爻
    lv_obj_t *hint_label;
} ly_cast_widgets_t;

// 手动录爻部件。
typedef struct {
    lv_obj_t *rows[LIUYAO_LINE_COUNT];
} ly_manual_widgets_t;

// 分类页部件(3×3 网格)。
typedef struct {
    lv_obj_t *cells[9];
} ly_grid_widgets_t;

// 日期页部件。
typedef struct {
    lv_obj_t *values[LY_DATE_FIELD_COUNT];
    lv_obj_t *titles[LY_DATE_FIELD_COUNT];
    lv_obj_t *marks[LY_DATE_FIELD_COUNT];  // 选中字段下的鎏金短横
} ly_date_widgets_t;

// 卦盘页部件(行部件由 build 直接创建,不保留句柄)。
// 解读页部件。
typedef struct {
    lv_obj_t *scroll;   // 滚动容器(按键驱动)
    lv_obj_t *body;
    lv_obj_t *page_label;
} ly_reading_widgets_t;

struct liyao_app_s {
    QueueHandle_t queue;
    lv_obj_t *screen;
    lv_obj_t *battery;
    ly_state_t state;
    bool switch_requested;   // 本轮输入已请求切页
    int sel;                 // 通用选中项
    int date_field;          // 日期页当前字段
    int manual_row;          // 手动录爻当前行
    int cast_count;          // 已摇出的爻数
    bool cast_busy;          // 摇卦动画进行中
    lv_timer_t *anim_timer;  // 摇卦动画定时器(切页前必须删)
    lv_timer_t *auto_timer;  // 六爻毕自动进入卦盘(同上)
    // 起卦时间(NVS 持久化)。
    int year, month, day, hour;
    liuyao_casting_t casting;
    bool casting_valid;
    liuyao_result_t result;
    bool reading_valid;  // reading_text 已对当前 result 生成过(翻页时直接复用)
    liuyao_reading_t reading_text;  // 解读分页文本缓冲(~3KB,常驻静态)
    char day_gz[10];    // "甲子"
    char month_gz[10];  // "丙寅"
    int reading_page;
    // 页面部件。
    ly_grid_widgets_t category;
    ly_grid_widgets_t perspective;
    ly_date_widgets_t date;
    ly_cast_widgets_t cast;
    ly_manual_widgets_t manual;
    ly_reading_widgets_t reading;
};

// 页面操作表:build/key 均在持有 bsp_lvgl_lock 时调用;
// exit 在删除屏幕前调用(清理定时器等)。
typedef struct {
    void (*build)(struct liyao_app_s *app);
    void (*key)(struct liyao_app_s *app, bsp_btn_t btn, bsp_btn_ev_t ev);
    void (*exit)(struct liyao_app_s *app);
} ly_page_ops_t;

// 三组页面的操作表(liuyao_pages_flow/cast/result.c 各实现其一)。
const ly_page_ops_t *liuyao_page_ops_flow(ly_state_t state);
const ly_page_ops_t *liuyao_page_ops_cast(ly_state_t state);
const ly_page_ops_t *liuyao_page_ops_result(ly_state_t state);

// LVGL 任务侧(定时器回调)通知应用任务立即处理切页请求。
void ly_app_notify(struct liyao_app_s *app);

// —— 供各页面共用的工具(liuyao_app.c 提供) ——
void ly_app_goto(struct liyao_app_s *app, ly_state_t next);  // 请求切页(删屏重建)
// 起卦失败的统一出口:置无效标记并回到起卦方式页,不进入需要 result 的页面。
void cast_failed(struct liyao_app_s *app);
// 依据 casting 计算 result。成功返回 true;失败时置 casting_valid=false。
bool ly_app_cast_lines_to_result(struct liyao_app_s *app);
bool ly_app_load_last_date(struct liyao_app_s *app);
void ly_app_save_last_date(const struct liyao_app_s *app);  // 异步:仅投递写请求
void ly_app_default_date(struct liyao_app_s *app);
int ly_days_in_month_clamped(int year, int month, int day);

// 页面内小组件:高亮面板选中态。
void ly_style_option(lv_obj_t *panel, bool selected);
