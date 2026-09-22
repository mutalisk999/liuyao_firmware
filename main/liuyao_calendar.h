// main/liuyao_calendar.h —— 六爻干支历法(纯逻辑,可主机测试,不依赖 IDF/LVGL)。
// 覆盖范围 2020-01-01 .. 2040-12-31(节气表按 lunar_python 1.4.8 生成)。
#pragma once

#include <stdbool.h>

// 支持的起卦日期范围(唯一真值:UI 边界、NVS 校验、默认日期都引用这里)。
#define LIUYAO_DATE_MIN_YEAR 2020
#define LIUYAO_DATE_MAX_YEAR 2040

// 干支下标约定:天干 甲=0..癸=9;地支 子=0..亥=11;六十甲子 甲子=0..癸亥=59。
typedef struct {
    int day_index;     // 日柱六十甲子下标(23 点起按子时政策归次日)
    int month_branch;  // 月柱地支(按节气,非农历月)
    int month_stem;    // 月柱天干(五虎遁)
    int year_stem;     // 年柱天干(立春分界)
    int year_branch;   // 年柱地支
} liuyao_date_facts_t;

// 日期是否在支持的范围内且月/日合法。
bool liuyao_date_supported(int year, int month, int day);

// 计算排盘所需历法事实。hour 取 0..23;hour>=23 按晚子时归入次日日柱。
// 日期或小时非法,或超出支持范围时返回 false 且不写 out。
bool liuyao_date_facts(int year, int month, int day, int hour,
                       liuyao_date_facts_t *out);

// 该年月的天数(支持公历常规规则,供日期选择器使用)。
int liuyao_days_in_month(int year, int month);
