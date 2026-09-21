// tests/test_liuyao_engine.c —— 六爻排盘引擎主机测试。
// 对 tests/liuyao_fixtures.h 的 330 个黄金向量(由 fortune-liuyao skill 的
// Python 引擎生成)逐字段对拍:输入 "L777777;d30;m5;c3;p1" 按同一指纹格式
// 重组输出并与期望串全等比较。
#include <stdio.h>
#include <string.h>

#include "liuyao_engine.h"
#include "liuyao_fixtures.h"

static const char *rel_letter(liuyao_relation_t rel) {
    switch (rel) {
        case LIUYAO_REL_SAME: return "s";
        case LIUYAO_REL_GENERATES: return "g";
        case LIUYAO_REL_CONTROLS: return "c";
        case LIUYAO_REL_GENERATED_BY: return "G";
        default: return "C";
    }
}

static const char *st_letter(liuyao_strength_t st) {
    switch (st) {
        case LIUYAO_STRENGTH_SUPPORTED: return "s";
        case LIUYAO_STRENGTH_WEAKENED: return "w";
        case LIUYAO_STRENGTH_CONTESTED: return "c";
        default: return "n";
    }
}

static int fingerprint(const liuyao_result_t *r, char *buf, size_t size) {
    const liuyao_chart_t *c = &r->chart;
    const liuyao_analysis_t *a = &r->analysis;
    size_t used = 0;
    used += (size_t)snprintf(buf + used, size - used,
                             "name=%s|up=%s|low=%s|palace=%s/%s/%s|shi=%d|ying=%d"
                             "|chg=%s|void=%s%s|pat=%c>%c|ys=%s",
                             c->name, c->upper_name, c->lower_name, c->palace,
                             c->palace_element, c->palace_stage, c->shi_pos,
                             c->ying_pos, c->changed_name,
                             liuyao_branch_str(c->void_branches[0]),
                             liuyao_branch_str(c->void_branches[1]),
                             c->pattern == LIUYAO_PATTERN_SIX_CLASH ? 'c'
                             : c->pattern == LIUYAO_PATTERN_SIX_HARMONY ? 'h' : 'o',
                             c->changed_pattern == LIUYAO_PATTERN_SIX_CLASH ? 'c'
                             : c->changed_pattern == LIUYAO_PATTERN_SIX_HARMONY ? 'h'
                                                                                : 'o',
                             a->yongshen ? a->yongshen : "-");
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        const liuyao_line_t *l = &c->lines[i];
        used += (size_t)snprintf(buf + used, size - used, "|%s%s%s%s%s|%c%c%c%c%c%c%s%s",
                                 l->stem, l->branch, l->element, l->relative,
                                 l->spirit,
                                 l->moving ? 'M' : 'm',
                                 l->is_shi ? 'S' : 's',
                                 l->is_ying ? 'Y' : 'y',
                                 l->is_void ? 'V' : 'v',
                                 l->is_month_break ? 'B' : 'b',
                                 l->is_day_clash ? 'D' : 'd',
                                 rel_letter(l->month_relation),
                                 rel_letter(l->day_relation));
        used += (size_t)snprintf(buf + used, size - used, "%s#", st_letter(l->strength));
        if (l->has_change) {
            used += (size_t)snprintf(buf + used, size - used, "%s%s%s%c%c",
                                     l->change_branch, l->change_element,
                                     l->change_relative,
                                     l->change_advance == 1 ? 'a'
                                     : l->change_advance == -1 ? 'r' : 'n',
                                     l->change_is_void ? 'V' : 'v');
        } else {
            used += (size_t)snprintf(buf + used, size - used, "-");
        }
    }
    if (c->hidden_count == 0) {
        used += (size_t)snprintf(buf + used, size - used, "|hid=-|");
    } else {
        used += (size_t)snprintf(buf + used, size - used, "|hid=");
        for (int i = 0; i < c->hidden_count; i++) {
            const liuyao_hidden_t *h = &c->hidden[i];
            used += (size_t)snprintf(buf + used, size - used, "%d%s%s%s,",
                                     h->position, h->branch, h->element, h->relative);
        }
        buf[used - 1] = '|';  // 把最后一个逗号换成竖线
    }
    used += (size_t)snprintf(buf + used, size - used, "cand=");
    if (a->candidate_count == 0) {
        used += (size_t)snprintf(buf + used, size - used, "-");
    } else {
        for (int i = 0; i < a->candidate_count; i++) {
            used += (size_t)snprintf(buf + used, size - used, "%s%d",
                                     i > 0 ? "," : "", a->candidates[i]);
        }
    }
    used += (size_t)snprintf(buf + used, size - used, ".");
    if (a->hidden_candidate_count == 0) {
        used += (size_t)snprintf(buf + used, size - used, "-");
    } else {
        for (int i = 0; i < a->hidden_candidate_count; i++) {
            used += (size_t)snprintf(buf + used, size - used, "%s%d",
                                     i > 0 ? "," : "", a->hidden_candidates[i]);
        }
    }
    return (int)used;
}

int main(void) {
    char got[1024];
    for (int s = 0; s < LIUYAO_ENGINE_SAMPLE_COUNT; s++) {
        const char *record = LIUYAO_ENGINE_SAMPLES[s];
        const char *hash = strchr(record, '#');
        if (!hash) return 100 + s;

        liuyao_casting_t casting;
        memset(&casting, 0, sizeof(casting));
        int parsed = sscanf(record,
                            "L%1d%1d%1d%1d%1d%1d;d%d;m%d;c%d;p%d",
                            &casting.lines[0], &casting.lines[1], &casting.lines[2],
                            &casting.lines[3], &casting.lines[4], &casting.lines[5],
                            &casting.day_index, &casting.month_branch,
                            (int *)&casting.category, (int *)&casting.perspective);
        if (parsed != 10) return 200 + s;

        liuyao_result_t result;
        if (!liuyao_cast(&casting, &result)) return 300 + s;

        fingerprint(&result, got, sizeof(got));
        const char *expected = hash + 1;
        if (strcmp(got, expected) != 0) {
            fprintf(stderr, "sample %d input %.24s\n  got: %s\n  want: %s\n",
                    s, record, got, expected);
            return 1;
        }
    }
    printf("engine golden: %d cases PASS\n", LIUYAO_ENGINE_SAMPLE_COUNT);
    return 0;
}
