// main/liuyao_reading.h —— 六爻解读文本生成(纯逻辑,可主机测试,不依赖 IDF/LVGL)。
// 依据排盘事实(用神旺衰、空破、动变、六冲六合)按固定模板生成分页解读。
#pragma once

#include <stdbool.h>

#include "liuyao_engine.h"

#define LIUYAO_READING_PAGE_COUNT 4
#define LIUYAO_READING_PAGE_BYTES 768  // 每页缓冲(UTF-8 字节,含结尾 NUL)

typedef struct {
    int page_count;                                   // 实际生成页数
    bool truncated;                                   // 任一页超出缓冲
    char pages[LIUYAO_READING_PAGE_COUNT][LIUYAO_READING_PAGE_BYTES];
} liuyao_reading_t;

// 生成分页解读。day_ganzhi/month_ganzhi 为显示用干支字串(如 "甲子"/"丙寅"),
// 可为 NULL。result 必须已经 liuyao_cast 成功生成。
void liuyao_compose_reading(const liuyao_result_t *result, const char *day_ganzhi,
                            const char *month_ganzhi, liuyao_reading_t *out);
