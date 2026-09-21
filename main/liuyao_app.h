// main/liuyao_app.h —— 六爻应用入口(BSP/LVGL 初始化完成后调用)。
#pragma once

// 创建应用任务并进入封面页。须在 bsp_display_init/bsp_lvgl_init 成功后调用;
// 内部自行初始化按键与 NVS 读取,失败时记录日志并保持当前画面。
void liuyao_app_start(void);
