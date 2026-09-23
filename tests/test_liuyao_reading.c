// tests/test_liuyao_reading.c —— 六爻文案数据与解读生成主机测试。
// 覆盖:64 卦卦意查表齐全性;对全部排盘黄金向量生成解读不越界不截断;
// 抽样校验解读内容包含关键事实字串。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "liuyao_data.h"
#include "liuyao_engine.h"
#include "liuyao_fixtures.h"
#include "liuyao_reading.h"

// 64 卦全部可查到关键词与卦意,未知卦名返回 NULL。
static void test_meaning_coverage(void) {
    int covered = 0;
    for (int upper = 0; upper < 8; upper++) {
        for (int lower = 0; lower < 8; lower++) {
            liuyao_casting_t casting;
            memset(&casting, 0, sizeof(casting));
            for (int i = 0; i < 6; i++) {
                int bit = i < 3 ? ((lower >> i) & 1) : ((upper >> (i - 3)) & 1);
                casting.lines[i] = bit ? 7 : 8;
            }
            casting.day_index = 0;
            casting.month_branch = 0;
            casting.category = LIUYAO_CAT_GENERAL;
            liuyao_result_t result;
            assert(liuyao_cast(&casting, &result));
            const char *name = result.chart.name;
            const char *keyword = liuyao_hexagram_keyword(name);
            const char *meaning = liuyao_hexagram_meaning(name);
            if (keyword == NULL || meaning == NULL) {
                fprintf(stderr, "missing meaning for %s\n", name);
                assert(keyword != NULL && meaning != NULL);
            }
            assert(liuyao_hexagram_keyword("不存在之卦") == NULL);
            covered++;
        }
    }
    assert(covered == 64);
    assert(liuyao_category_label(LIUYAO_CAT_GENERAL) != NULL);
    assert(liuyao_category_label(LIUYAO_CAT_FAMILY) != NULL);
    assert(liuyao_category_label(LIUYAO_CAT_COUNT) == NULL);
    assert(liuyao_perspective_label(LIUYAO_PERSP_UNSPECIFIED) != NULL);
    assert(liuyao_perspective_label(LIUYAO_PERSP_FEMALE) != NULL);
}

// 对黄金向量全集生成解读:必须 4 页、无截断、页非空。
static void test_readings_bounded(void) {
    for (int s = 0; s < LIUYAO_ENGINE_SAMPLE_COUNT; s++) {
        const char *record = LIUYAO_ENGINE_SAMPLES[s];
        liuyao_casting_t casting;
        memset(&casting, 0, sizeof(casting));
        int parsed = sscanf(record, "L%1d%1d%1d%1d%1d%1d;d%d;m%d;c%d;p%d",
                            &casting.lines[0], &casting.lines[1], &casting.lines[2],
                            &casting.lines[3], &casting.lines[4], &casting.lines[5],
                            &casting.day_index, &casting.month_branch,
                            (int *)&casting.category, (int *)&casting.perspective);
        assert(parsed == 10);
        liuyao_result_t result;
        assert(liuyao_cast(&casting, &result));
        liuyao_reading_t reading;
        liuyao_compose_reading(&result, "甲子", "丙寅", &reading);
        if (reading.truncated) {
            fprintf(stderr, "reading truncated at sample %d: %.28s\n", s, record);
            assert(!reading.truncated);
        }
        assert(reading.page_count == LIUYAO_READING_PAGE_COUNT);
        for (int p = 0; p < reading.page_count; p++) {
            assert(reading.pages[p][0] != '\0');
        }
    }
}

static liuyao_result_t cast_lines(const int *lines, int day, int month_branch,
                                  liuyao_category_t category,
                                  liuyao_perspective_t perspective) {
    liuyao_casting_t casting;
    memset(&casting, 0, sizeof(casting));
    memcpy(casting.lines, lines, sizeof(casting.lines));
    casting.day_index = day;
    casting.month_branch = month_branch;
    casting.category = category;
    casting.perspective = perspective;
    liuyao_result_t result;
    assert(liuyao_cast(&casting, &result));
    return result;
}

// 内容抽样:静卦/动卦/伏神/六冲六合互变各验一处关键事实。
static void test_reading_content(void) {
    liuyao_reading_t reading;

    // 静卦乾为天。
    const int all7[6] = {7, 7, 7, 7, 7, 7};
    liuyao_result_t r = cast_lines(all7, 0, 0, LIUYAO_CAT_GENERAL,
                                   LIUYAO_PERSP_UNSPECIFIED);
    liuyao_compose_reading(&r, "甲子", "丙寅", &reading);
    assert(strstr(reading.pages[0], "乾为天") != NULL);
    assert(strstr(reading.pages[0], "六爻安静") != NULL);
    assert(strstr(reading.pages[1], "世爻在6爻") != NULL);
    assert(strstr(reading.pages[3], "参考") != NULL);

    // 乾之姤:初爻动,用神妻财(求财)。
    const int qian_gou[6] = {9, 7, 7, 7, 7, 7};
    r = cast_lines(qian_gou, 0, 2, LIUYAO_CAT_WEALTH, LIUYAO_PERSP_UNSPECIFIED);
    liuyao_compose_reading(&r, NULL, NULL, &reading);
    assert(strstr(reading.pages[0], "天风姤") != NULL);
    assert(strstr(reading.pages[2], "动化") != NULL);
    assert(strstr(reading.pages[2], "应期参考") != NULL);

    // 坤为地全动(老阴)变乾为地? -> 坤之乾,女问感情用官鬼。
    const int all6[6] = {6, 6, 6, 6, 6, 6};
    r = cast_lines(all6, 30, 5, LIUYAO_CAT_RELATION, LIUYAO_PERSP_FEMALE);
    liuyao_compose_reading(&r, "甲午", "癸酉", &reading);
    assert(strstr(reading.pages[1], "官鬼") != NULL);
    assert(strstr(reading.pages[3], "综合倾向") != NULL);

    // 出行:用神为世爻。
    const int travel_lines[6] = {7, 8, 8, 8, 8, 8};
    r = cast_lines(travel_lines, 12, 2, LIUYAO_CAT_TRAVEL, LIUYAO_PERSP_UNSPECIFIED);
    liuyao_compose_reading(&r, NULL, NULL, &reading);
    assert(strstr(reading.pages[1], "【用神】世爻") != NULL);

    // 全部 9 类目都可生成且含结论页。
    for (int cat = 0; cat < LIUYAO_CAT_COUNT; cat++) {
        const int lines[6] = {8, 8, 8, 8, 8, 8};
        r = cast_lines(lines, 5, 1, (liuyao_category_t)cat,
                       LIUYAO_PERSP_UNSPECIFIED);
        liuyao_compose_reading(&r, NULL, NULL, &reading);
        assert(strstr(reading.pages[3], "综合倾向") != NULL);
    }
}

int main(void) {
    test_meaning_coverage();
    test_readings_bounded();
    test_reading_content();
    printf("reading tests: PASS\n");
    return 0;
}
