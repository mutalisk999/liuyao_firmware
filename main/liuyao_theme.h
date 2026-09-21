// main/liuyao_theme.h —— 六爻应用视觉主题:配色、字体、公共组件。
// 独立于 baseline demo 的 ui_pixel 外壳(衍生应用强制 UI 重设计)。
#pragma once

#include "lvgl.h"

// 墨夜 + 宣纸 + 朱砂 + 鎏金的玄学配色。
#define LY_COLOR_BG      0x14121A  // 玄墨底
#define LY_COLOR_PANEL   0x201D2B  // 面板
#define LY_COLOR_PANEL_2 0x2A2638  // 面板高亮
#define LY_COLOR_PAPER   0xEDE3D0  // 宣纸字色
#define LY_COLOR_PAPER_DIM 0x9A90A6  // 次要字色
#define LY_COLOR_CINNABAR 0xC03A2B  // 朱砂
#define LY_COLOR_GOLD    0xD9A441  // 鎏金
#define LY_COLOR_YANG    0xEDE3D0  // 阳爻
#define LY_COLOR_YIN     0x6B637E  // 阴爻

// 应用字库(assets/fonts 生成,见 tools/gen_liuyao_fonts.sh)。
LV_FONT_DECLARE(liuyao_font_16);
LV_FONT_DECLARE(liuyao_font_24);
LV_FONT_DECLARE(liuyao_font_48);

// 页面脚手架:深底 + 标题 + 右上角电量。返回页面根对象(全屏)。
lv_obj_t *liuyao_page_create(const char *title);

// 右上角电量组件刷新;soc<0 时显示 "--"。
void liuyao_battery_update(lv_obj_t *battery_label, int soc);

// 一条卦爻(自绘):yang=true 画整条,否则画两段。moving 时在右侧画 o/x 标记。
lv_obj_t *liuyao_yao_create(lv_obj_t *parent, int x, int y, int width,
                            bool yang, bool moving, bool dim);

// 简化太极图(装饰):圆盘 + 双鱼 + 两点,纯 lv_obj 几何拼装。
lv_obj_t *liuyao_taiji_create(lv_obj_t *parent, int cx, int cy, int radius);
