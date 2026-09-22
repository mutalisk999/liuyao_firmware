// main/liuyao_app.c —— 六爻应用主任务:按键队列、页面切换、起卦数据准备。
// 运行模型沿用仓库基线:按键回调只入队;本任务在 bsp_lvgl_lock 下操作 LVGL。
#include "liuyao_app.h"

#include <stdio.h>
#include <string.h>

#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "liuyao_app_internal.h"
#include "liuyao_calendar.h"
#include "liuyao_theme.h"
#include "nvs.h"

static const char *TAG = "liuyao";

#define LY_INPUT_QUEUE_DEPTH 8
#define LY_BATTERY_POLL_MS 6000
#define LY_NVS_NAMESPACE "liuyao"
#define LY_NVS_KEY_DATE "last_date"
#define LY_TASK_STACK_BYTES 6144
#define LY_TASK_PRIORITY 5
// SOC 读取与 NVS 写入是慢操作:不能在持有 bsp_lvgl_lock 时执行,
// 否则 LVGL 渲染任务会被 I2C/闪存时延卡住。
#define LY_SLOW_OP_TICKS pdMS_TO_TICKS(100)

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t ev;
} ly_input_event_t;

// 队列唤醒标记:定时器回调发起切页后叫醒应用任务(事件本身无按键语义)。
// 取 -1 是因为 bsp_btn_ev_t 的合法取值是 0..3,-1 永不会与真实按键事件撞车;
// 显式命名出来,避免阅读时误以为这是个笔误。
#define LY_EV_WAKE ((bsp_btn_ev_t)-1)

static struct liyao_app_s s_app;
static ly_state_t s_next_state;
static QueueHandle_t s_queue;
// 待写的上次起卦日期:页面在锁内只置位,应用任务解锁后再做闪存写。
static bool s_pending_save;

// 单一入口查找某状态的页面操作表;三组 provider 各自只认自己的状态。
static const ly_page_ops_t *page_ops(ly_state_t state) {
    if ((int)state < 0 || (int)state >= LY_STATE_COUNT) return NULL;
    const ly_page_ops_t *ops = liuyao_page_ops_flow(state);
    if (!ops) ops = liuyao_page_ops_cast(state);
    if (!ops) ops = liuyao_page_ops_result(state);
    return ops;
}

// 编译期保证:新增状态必须在某张 provider 表里登记,
// 否则 enter_state() 会落入“无页面”分支并告警退回封面。
_Static_assert(LY_STATE_COUNT == 9, "新增页面状态必须同时在三组 provider 中登记");

void ly_style_option(lv_obj_t *panel, bool selected) {
    // 选中:宣纸底 + 朱砂字与描边 + 投影浮起;未选中:墨底面板 + 宣纸字。
    // (深底上默认黑字不可读,故未选中态显式用宣纸字色。)
    if (selected) {
        lv_obj_set_style_bg_color(panel, lv_color_hex(LY_COLOR_PAPER), 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(panel, lv_color_hex(LY_COLOR_CINNABAR), 0);
        lv_obj_set_style_border_width(panel, 2, 0);
        lv_obj_set_style_border_color(panel, lv_color_hex(LY_COLOR_CINNABAR), 0);
        lv_obj_set_style_radius(panel, 6, 0);
        lv_obj_set_style_shadow_color(panel, lv_color_hex(LY_COLOR_CINNABAR), 0);
        lv_obj_set_style_shadow_opa(panel, LV_OPA_40, 0);
        lv_obj_set_style_shadow_width(panel, 10, 0);
        lv_obj_set_style_shadow_spread(panel, 1, 0);
    } else {
        lv_obj_set_style_bg_color(panel, lv_color_hex(LY_COLOR_PANEL), 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(panel, lv_color_hex(LY_COLOR_PAPER), 0);
        lv_obj_set_style_border_width(panel, 1, 0);
        lv_obj_set_style_border_color(panel, lv_color_hex(LY_COLOR_PANEL_2), 0);
        lv_obj_set_style_radius(panel, 6, 0);
        lv_obj_set_style_shadow_width(panel, 0, 0);
        lv_obj_set_style_shadow_opa(panel, LV_OPA_TRANSP, 0);
    }
}

int ly_days_in_month_clamped(int year, int month, int day) {
    int dim = liuyao_days_in_month(year, month);
    if (day > dim) return dim;
    return day < 1 ? 1 : day;
}

void ly_app_default_date(struct liyao_app_s *app) {
    // 首次启动:取编译日期(与发布时间接近),之后由 NVS 记忆上次输入。
    static const char *const k_months[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    char month[4] = {0};
    int day = 1;
    int year = 2026;
    sscanf(__DATE__, "%3s %d %d", month, &day, &year);
    int month_index = 0;
    for (int i = 0; i < 12; i++) {
        if (strncmp(month, k_months[i], 3) == 0) month_index = i + 1;
    }
    app->year = year >= LIUYAO_DATE_MIN_YEAR && year <= LIUYAO_DATE_MAX_YEAR
                    ? year
                    : 2026;
    app->month = month_index >= 1 && month_index <= 12 ? month_index : 1;
    app->day = day >= 1 && day <= 31 ? day : 1;
    app->hour = 12;
}

bool ly_app_load_last_date(struct liyao_app_s *app) {
    nvs_handle_t handle;
    if (nvs_open(LY_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;
    int32_t packed[4];
    size_t size = sizeof(packed);
    bool ok = nvs_get_blob(handle, LY_NVS_KEY_DATE, packed, &size) == ESP_OK &&
              size == sizeof(packed);
    nvs_close(handle);
    if (!ok) return false;
    // 四字段全校验:NVS 可能残留其它 schema 的数据或旧版本格式。
    // 只校验年份会让 month=13/hour=25 之类的值流入起卦路径。
    int year = packed[0], month = packed[1], day = packed[2], hour = packed[3];
    if (year < LIUYAO_DATE_MIN_YEAR || year > LIUYAO_DATE_MAX_YEAR) return false;
    if (month < 1 || month > 12) return false;
    if (hour < 0 || hour > 23) return false;
    if (day < 1 || day > liuyao_days_in_month(year, month)) return false;
    app->year = year;
    app->month = month;
    app->day = day;
    app->hour = hour;
    return true;
}

// 实际的 NVS 写入:在应用任务中、不持有 LVGL 锁时执行。
static void save_last_date_task(const struct liyao_app_s *app) {
    nvs_handle_t handle;
    if (nvs_open(LY_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    int32_t packed[4] = {app->year, app->month, app->day, app->hour};
    if (nvs_set_blob(handle, LY_NVS_KEY_DATE, packed, sizeof(packed)) == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);
}

void ly_app_save_last_date(const struct liyao_app_s *app) {
    // 页面在锁内调用:只投递请求,真正的闪存写入在应用任务解锁后完成。
    s_pending_save = true;
}

static void refresh_battery(struct liyao_app_s *app) {
    if (app->battery) {
        liuyao_battery_update(app->battery, bsp_battery_soc());
    }
}
// 依据 casting(爻值+类别+视角)与所选时间计算历法事实与卦象。
// 成功时置 casting_valid;失败时返回 false,调用方必须据此决定去向。
bool ly_app_cast_lines_to_result(struct liyao_app_s *app) {
    liuyao_date_facts_t facts;
    if (!liuyao_date_facts(app->year, app->month, app->day, app->hour, &facts)) {
        ESP_LOGE(TAG, "起卦时间不受支持: %d-%d-%d %d时", app->year, app->month,
                 app->day, app->hour);
        app->casting_valid = false;
        return false;
    }
    app->casting.day_index = facts.day_index;
    app->casting.month_branch = facts.month_branch;
    snprintf(app->day_gz, sizeof(app->day_gz), "%s%s",
             liuyao_stem_str(facts.day_index % 10),
             liuyao_branch_str(facts.day_index % 12));
    snprintf(app->month_gz, sizeof(app->month_gz), "%s%s",
             liuyao_stem_str(facts.month_stem),
             liuyao_branch_str(facts.month_branch));
    if (!liuyao_cast(&app->casting, &app->result)) {
        ESP_LOGE(TAG, "排盘失败");
        app->casting_valid = false;
        return false;
    }
    app->casting_valid = true;
    app->reading_valid = false;  // 结果已变,解读文本需要重算
    return true;
}

// 切页请求:仅置位,实际删屏重建在本轮锁内完成(见 process_event)。
// 若队列满导致 WAKE 丢失,定时轮询分支会在下一轮补做切换,
// 不会出现 switch_requested 永久卡住、按键全部被丢弃的情形。
void ly_app_goto(struct liyao_app_s *app, ly_state_t next) {
    s_next_state = next;
    app->switch_requested = true;
}

// 起卦失败时的统一出口:不进入需要 result 的页面,回到起卦方式页,
// 让用户改时间或重录,而不是带着全零的 chart 去渲染 NULL 字符串。
void cast_failed(struct liyao_app_s *app) {
    app->casting_valid = false;
    ESP_LOGE(TAG, "排盘失败,已返回起卦方式页");
    ly_app_goto(app, LY_STATE_METHOD);
}

static void enter_state(struct liyao_app_s *app, ly_state_t state) {
    app->state = state;
    app->screen = NULL;
    app->battery = NULL;
    // 上一次页面的控件句柄随屏幕删除已失效:全部清空,避免悬空指针。
    memset(&app->category, 0, sizeof(app->category));
    memset(&app->perspective, 0, sizeof(app->perspective));
    memset(&app->date, 0, sizeof(app->date));
    memset(&app->cast, 0, sizeof(app->cast));
    memset(&app->manual, 0, sizeof(app->manual));
    memset(&app->reading, 0, sizeof(app->reading));
    const ly_page_ops_t *ops = page_ops(state);
    if (ops && ops->build) {
        ops->build(app);
    }
    if (!app->screen) {
        // 某状态没有 ops 表项,或 build 因内存不足未创建出屏幕:
        // 明确告警并退回封面,而不是留一张空屏让后续按键操作悬空指针。
        ESP_LOGE(TAG, "状态 %d 无可用页面或页面构建失败,退回封面", (int)state);
        ops = page_ops(LY_STATE_HOME);
        if (ops && ops->build) ops->build(app);
        app->state = app->screen ? LY_STATE_HOME : app->state;
    }
    // 规范默认位:页面右上角电量(左移避开四角角饰)。
    if (app->screen) {
        app->battery = lv_label_create(app->screen);
        if (app->battery) {
            lv_obj_set_style_text_font(app->battery, &liuyao_font_16, 0);
            lv_obj_set_style_text_color(app->battery,
                                        lv_color_hex(LY_COLOR_PAPER_DIM), 0);
            lv_obj_set_pos(app->battery, 182, 10);
        }
        refresh_battery(app);
        lv_screen_load(app->screen);
    }
}

static void apply_switch(struct liyao_app_s *app) {
    ly_state_t next = s_next_state;
    app->switch_requested = false;
    const ly_page_ops_t *ops = page_ops(app->state);
    if (ops && ops->exit) {
        ops->exit(app);
    }
    if (app->screen) {
        lv_obj_delete(app->screen);
        app->screen = NULL;
        app->battery = NULL;
    }
    enter_state(app, next);
}

static void process_event(struct liyao_app_s *app, const ly_input_event_t *event) {
    if (event->ev == LY_EV_WAKE) {
        if (app->switch_requested) {
            apply_switch(app);
        }
        return;
    }
    if (app->switch_requested) {
        // 上一次切页尚未执行(例如 WAKE 在队列满时被丢弃)。
        // 直接在此补做,而不是丢弃本次按键。
        apply_switch(app);
        if (app->switch_requested) return;  // 补做仍失败,放弃本事件
    }
    const ly_page_ops_t *ops = page_ops(app->state);
    if (ops && ops->key) {
        ops->key(app, event->btn, event->ev);
    }
    if (app->switch_requested) {
        apply_switch(app);
    }
}

void ly_app_notify(struct liyao_app_s *app) {
    if (app->queue) {
        const ly_input_event_t wake = {.btn = BSP_BTN_UP, .ev = LY_EV_WAKE};
        // 允许短暂阻塞:队列满时等待一个 tick,换取 WAKE 不丢。
        // WAKE 丢失会让 switch_requested 卡住(见 process_event的补做兜底)。
        (void)xQueueSend(app->queue, &wake, LY_SLOW_OP_TICKS);
    }
}

static void app_task(void *arg) {
    struct liyao_app_s *app = (struct liyao_app_s *)arg;
    ly_input_event_t event;
    int last_soc = -2;  // -2 表示尚未渲染过,与"读失败 -1"区分
    for (;;) {
        if (xQueueReceive(app->queue, &event, pdMS_TO_TICKS(LY_BATTERY_POLL_MS)) ==
            pdTRUE) {
            // LVGL 非线程安全:页面重建/控件更新一律持锁。
            // 拿不到锁说明 LVGL 任务没有让出(例如重绘卡死),此时事件会被丢弃;
            // 连续失败时明确告警,避免问题表现为“按键毫无反应”而无任何日志。
            static int lock_stall;
            if (bsp_lvgl_lock(500)) {
                lock_stall = 0;
                process_event(app, &event);
                // 慢操作(I2C/闪存)在解锁后执行,避免卡住 LVGL 渲染任务。
                if (s_pending_save) {
                    s_pending_save = false;
                    bsp_lvgl_unlock();
                    save_last_date_task(app);
                } else {
                    bsp_lvgl_unlock();
                }
            } else if (lock_stall++ == 2) {
                ESP_LOGE(TAG, "LVGL 锁持续不可用,输入事件已开始丢弃");
            }
        } else {
            // 定时轮询分支:补做可能丢失的切页请求(WAKE 丢包兜底)。
            if (app->switch_requested) {
                if (bsp_lvgl_lock(500)) {
                    apply_switch(app);
                    bsp_lvgl_unlock();
                }
                continue;
            }
            // SOC 读取先在锁外做(I2C 时延不阻塞渲染),再持锁刷新标签。
            int soc = bsp_battery_soc();
            if (soc == last_soc) continue;
            last_soc = soc;
            if (bsp_lvgl_lock(200)) {
                refresh_battery(app);
                bsp_lvgl_unlock();
            }
        }
    }
}

// 按键回调运行在共享 esp_timer 任务:只入队,立即返回。
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    const ly_input_event_t event = {.btn = btn, .ev = ev};
    (void)xQueueSend(s_queue, &event, 0);
}

void liuyao_app_start(void) {
    memset(&s_app, 0, sizeof(s_app));
    if (!ly_app_load_last_date(&s_app)) {
        ly_app_default_date(&s_app);
    }

    s_queue = xQueueCreate(LY_INPUT_QUEUE_DEPTH, sizeof(ly_input_event_t));
    // 先赋值再建任务:应用任务优先级更高,创建后立即调度,
    // 不能让它看到尚未赋值的 queue。
    s_app.queue = s_queue;
    if (!s_queue || xTaskCreate(app_task, "liuyao_app", LY_TASK_STACK_BYTES, &s_app,
                                LY_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "应用任务创建失败");
        return;
    }

    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "按键初始化失败,应用无法交互");
        return;
    }

    if (bsp_lvgl_lock(1000)) {
        enter_state(&s_app, LY_STATE_HOME);
        bsp_lvgl_unlock();
    } else {
        ESP_LOGE(TAG, "LVGL 锁获取失败");
    }
    ESP_LOGI(TAG, "六爻应用就绪(默认起卦日期 %d-%d-%d %d时)", s_app.year,
             s_app.month, s_app.day, s_app.hour);
}
