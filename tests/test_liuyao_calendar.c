// tests/test_liuyao_calendar.c —— 六爻历法主机测试。
// 对拍基准 tests/liuyao_fixtures.h 由 lunar_python 1.4.8 导出生成
// (tools/gen_liuyao_fixtures.py),覆盖 2020..2040 常规采样与节气边界 ±1 天。
#include <assert.h>

#include "liuyao_calendar.h"
#include "liuyao_fixtures.h"

int main(void) {
    for (int i = 0; i < LIUYAO_CAL_SAMPLE_COUNT; i++) {
        const liuyao_cal_sample_t *s = &LIUYAO_CAL_SAMPLES[i];
        assert(liuyao_date_supported(s->year, s->month, s->day));
        liuyao_date_facts_t facts;
        assert(liuyao_date_facts(s->year, s->month, s->day, s->hour, &facts));
        if (facts.day_index != s->day_index) return 1;
        if (facts.month_branch != s->month_branch) return 2;
        if (facts.month_stem != s->month_stem) return 3;
        if (facts.year_stem != s->year_stem) return 4;
        if (facts.year_branch != s->year_branch) return 5;
    }

    // 越界与非法输入。
    liuyao_date_facts_t facts;
    assert(!liuyao_date_supported(2019, 12, 31));
    assert(!liuyao_date_supported(2041, 1, 1));
    assert(!liuyao_date_supported(2024, 2, 30));
    assert(!liuyao_date_supported(2024, 13, 1));
    assert(!liuyao_date_facts(2024, 5, 1, 24, &facts));
    assert(!liuyao_date_facts(2024, 5, 1, -1, &facts));
    assert(!liuyao_date_facts(2019, 5, 1, 12, &facts));
    assert(liuyao_days_in_month(2024, 2) == 29);
    assert(liuyao_days_in_month(2023, 2) == 28);
    assert(liuyao_days_in_month(2023, 12) == 31);
    return 0;
}
