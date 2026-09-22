// main/liuyao_reading.c —— 六爻解读文本生成。
// 只使用排盘引擎输出的确定性事实;吉凶表述为相对倾向,并附文化参考声明。
#include "liuyao_reading.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "liuyao_data.h"

// 六合:子丑 寅亥 卯戌 辰酉 巳申 午未。
static const int k_harmony[12] = {1, 0, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2};

typedef struct {
    char *buf;
    size_t size;
    size_t used;
    bool truncated;
} appender_t;

static void apf(appender_t *a, const char *fmt, ...) {
    if (a->used + 1 >= a->size) {
        a->truncated = true;
        return;
    }
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(a->buf + a->used, a->size - a->used, fmt, args);
    va_end(args);
    if (n < 0) {
        a->truncated = true;
        return;
    }
    if ((size_t)n >= a->size - a->used) {
        a->used = a->size - 1;
        a->truncated = true;
        return;
    }
    a->used += (size_t)n;
}

// 主用神爻位:先取第一位明露候选,否则第一位伏神。无返回 -1。
static int primary_candidate(const liuyao_analysis_t *a) {
    if (a->candidate_count > 0) return a->candidates[0];
    if (a->hidden_candidate_count > 0) return a->hidden_candidates[0];
    return -1;
}

// 世应生克描述(以五行论)。
static const char *shi_ying_relation(const liuyao_chart_t *c) {
    int shi_elem = liuyao_branch_element(c->lines[c->shi_pos - 1].branch_index);
    int ying_elem = liuyao_branch_element(c->lines[c->ying_pos - 1].branch_index);
    if (shi_elem < 0 || ying_elem < 0) return "";
    if (shi_elem == ying_elem) return "世应比和，彼此相安。";
    if ((shi_elem + 1) % 5 == ying_elem) return "世生应，我付出较多。";
    if ((shi_elem + 2) % 5 == ying_elem) return "世克应，我可主动掌控。";
    if ((ying_elem + 1) % 5 == shi_elem) return "应生世，对方助我。";
    return "应克世，对方制我，宜谨慎应对。";
}

static void page_chart(const liuyao_result_t *r, const char *day_gz,
                       const char *month_gz, appender_t *a) {
    const liuyao_chart_t *c = &r->chart;
    int moving[LIUYAO_LINE_COUNT];
    int moving_count = 0;
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        if (c->lines[i].moving) moving[moving_count++] = i;
    }

    apf(a, "【卦象】%s · %s\n%s。\n", c->name, liuyao_hexagram_keyword(c->name),
        liuyao_hexagram_meaning(c->name));
    if (month_gz || day_gz) {
        apf(a, "起卦:%s月 %s日\n", month_gz ? month_gz : "?", day_gz ? day_gz : "?");
    }
    if (moving_count == 0) {
        apf(a, "六爻安静，为静卦。\n");
    } else {
        apf(a, "动爻:");
        for (int i = 0; i < moving_count; i++) {
            apf(a, "%s%d爻", i > 0 ? "、" : "", moving[i] + 1);
        }
        apf(a, "。\n【变卦】%s · %s\n%s。\n", c->changed_name,
            liuyao_hexagram_keyword(c->changed_name),
            liuyao_hexagram_meaning(c->changed_name));
    }

    // [本卦卦式][变卦卦式]:0 平卦 1 六冲 2 六合。
    static const char *const k_pattern_lines[3][3] = {
        {"无六冲六合格局。", "逢六冲:主散主快，事多波折聚散。",
         "逢六合:主合主成，多有和合之机。"},
        {"六冲击变平卦:动荡渐平。", "冲中逢冲:事多反复，来去皆快。",
         "六合变六冲:先合后散，防中道生变。"},
        {"六合化平:和合之势转淡。", "六冲变六合:先散后聚，终得和合。",
         "合处逢合:情谊缠绵，然拖延难决。"},
    };
    apf(a, "%s\n", k_pattern_lines[c->pattern][c->changed_pattern]);
}

static void page_yongshen(const liuyao_result_t *r, appender_t *a) {
    const liuyao_chart_t *c = &r->chart;
    const liuyao_analysis_t *an = &r->analysis;
    const liuyao_line_t *shi = &c->lines[c->shi_pos - 1];
    const liuyao_line_t *ying = &c->lines[c->ying_pos - 1];

    if (an->yongshen == NULL) {
        apf(a, "【主线】%s\n", liuyao_category_focus(an->category));
    } else {
        apf(a, "【用神】%s\n", an->yongshen);
        int pos = primary_candidate(an);
        if (pos < 0) {
            apf(a, "卦中未见用神。\n");
        } else {
            bool hidden = an->candidate_count == 0;
            const liuyao_line_t *line = &c->lines[pos - 1];
            if (hidden) {
                apf(a, "用神不现，伏于%d爻之下(%s%s)，未得引拔。\n", pos,
                    line->branch, line->element);
            } else {
                apf(a, "明现于%d爻，%s%s%s，%s。\n", pos, line->branch,
                    line->element, line->relative ? line->relative : "",
                    line->moving ? "发动" : "安静");
            }
            if (line->is_void) apf(a, "值旬空，力量暂不落实。\n");
            if (line->is_month_break) apf(a, "逢月破，本月中受损。\n");
            if (!hidden && line->is_day_clash && !line->moving) {
                apf(a, "被日辰冲，静者暗动。\n");
            }
            apf(a, "月建日辰:%s。\n",
                line->strength == LIUYAO_STRENGTH_SUPPORTED
                    ? "生扶用神，旺相有力"
                    : line->strength == LIUYAO_STRENGTH_WEAKENED
                          ? "克抑用神，衰弱受制"
                          : line->strength == LIUYAO_STRENGTH_CONTESTED
                                ? "有生有克，旺衰相抵"
                                : "平平相持，不生不克");
        }
    }
    apf(a, "世爻在%d爻(%s%s%s)，应爻在%d爻(%s%s%s)。%s\n", c->shi_pos,
        shi->relative, shi->branch, shi->element, c->ying_pos, ying->relative,
        ying->branch, ying->element, shi_ying_relation(c));
}

// 变化判词:化进退 > 化空 > 回头生克 > 同气/他论。
static const char *change_verdict(const liuyao_line_t *l) {
    if (l->change_advance == 1) return "化进神，势头渐旺。";
    if (l->change_advance == -1) return "化退神，势头渐弱。";
    if (l->change_is_void) return "化空，一时落空，出空方应。";
    int from = liuyao_branch_element(l->branch_index);
    int to = liuyao_branch_element(l->change_branch_index);
    if (from < 0 || to < 0) return "";
    if ((to + 1) % 5 == from) return "回头生，渐有转机。";
    if ((to + 2) % 5 == from) return "回头克，防反复受阻。";
    if (to == from) return "化同气，其势依旧。";
    return "化泄他气，力量转移。";
}

// 动爻对目标爻(世爻/用神)的作用:1 生扶, -1 克伤, 0 无明显作用。
static int action_to(const liuyao_line_t *mover, int target_branch) {
    int me = liuyao_branch_element(mover->branch_index);
    int tgt = liuyao_branch_element(target_branch);
    if (me < 0 || tgt < 0) return 0;
    if ((me + 1) % 5 == tgt) return 1;
    if ((me + 2) % 5 == tgt) return -1;
    return 0;
}

static void page_moving(const liuyao_result_t *r, appender_t *a) {
    const liuyao_chart_t *c = &r->chart;
    const liuyao_analysis_t *an = &r->analysis;
    int shi_branch = c->lines[c->shi_pos - 1].branch_index;
    int yong_pos = primary_candidate(an);
    int yong_branch = yong_pos > 0 ? c->lines[yong_pos - 1].branch_index : -1;

    apf(a, "【动爻】\n");
    int described = 0;
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        const liuyao_line_t *l = &c->lines[i];
        if (!l->moving) continue;
        described++;
        apf(a, "%s%s(%s)动化%s%s:", l->relative, l->branch, l->spirit,
            l->change_relative, l->change_branch);
        apf(a, "%s\n", change_verdict(l));
        int to_shi = action_to(l, shi_branch);
        if (to_shi == 1 && !l->is_shi) {
            apf(a, "此爻生扶世爻。\n");
        } else if (to_shi == -1 && !l->is_shi) {
            apf(a, "此爻克伤世爻。\n");
        }
        if (yong_pos > 0 && i + 1 != yong_pos && !l->is_shi) {
            int to_yong = action_to(l, yong_branch);
            if (to_yong == 1) {
                apf(a, "此爻生扶用神。\n");
            } else if (to_yong == -1) {
                apf(a, "此爻克伤用神。\n");
            }
        }
    }
    if (described == 0) {
        apf(a, "六爻安静，事体平稳，以月日与用神旺衰为主断。\n");
    }

    // 应期候选(仅列参考,不做确定承诺)。
    apf(a, "【应期参考】\n");
    if (yong_pos > 0) {
        const liuyao_line_t *line = &c->lines[yong_pos - 1];
        const char *br = line->branch;
        const char *clash = liuyao_branch_str((line->branch_index + 6) % 12);
        const char *harmony = liuyao_branch_str(k_harmony[line->branch_index]);
        bool any = false;
        if (an->candidate_count == 0) {
            apf(a, "用神伏藏，冲去飞神(%s)之日可见端倪。\n",
                line->branch);  // line 即飞神位
            any = true;
        }
        if (line->is_void) {
            apf(a, "%s日填实出空，或%s日冲空而应。\n", br, clash);
            any = true;
        }
        if (line->is_month_break) {
            apf(a, "%s日实破，或%s日逢合有望。\n", br, harmony);
            any = true;
        }
        if (line->moving && !any) {
            apf(a, "%s日逢值，%s日逢合，为发动之应。\n", br, harmony);
            any = true;
        }
        if (!any) {
            apf(a, "以%s日或%s日(冲应)为参考。\n", br, clash);
        }
    } else {
        apf(a, "以世爻%s日值期为参考。\n", c->lines[c->shi_pos - 1].branch);
    }
}

// 综合倾向打分:正值向好,负值受阻。只做相对倾向,不断定吉凶。
static int tendency_score(const liuyao_result_t *r) {
    const liuyao_chart_t *c = &r->chart;
    const liuyao_analysis_t *an = &r->analysis;
    int score = 0;
    int pos = primary_candidate(an);
    if (pos > 0) {
        const liuyao_line_t *l = &c->lines[pos - 1];
        if (l->strength == LIUYAO_STRENGTH_SUPPORTED) score += 3;
        if (l->strength == LIUYAO_STRENGTH_WEAKENED) score -= 3;
        if (l->is_void) score -= 2;
        if (l->is_month_break) score -= 2;
        if (l->moving) score += 1;
        if (an->candidate_count == 0) score -= 1;  // 用神伏藏
        if (pos == c->shi_pos) score += 1;         // 用神临世:自己掌权
        if (pos == c->ying_pos) score += 1;        // 用神临应:对方主动
    }
    if (c->pattern == LIUYAO_PATTERN_SIX_HARMONY) score += 2;
    if (c->pattern == LIUYAO_PATTERN_SIX_CLASH) score -= 2;
    if (c->pattern == LIUYAO_PATTERN_SIX_CLASH &&
        c->changed_pattern == LIUYAO_PATTERN_SIX_HARMONY) {
        score += 1;  // 冲中变合,先难后顺
    }
    if (c->pattern == LIUYAO_PATTERN_SIX_HARMONY &&
        c->changed_pattern == LIUYAO_PATTERN_SIX_CLASH) {
        score -= 1;  // 合中变冲,防生变
    }
    int shi_branch = c->lines[c->shi_pos - 1].branch_index;
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        if (!c->lines[i].moving) continue;
        int act = action_to(&c->lines[i], shi_branch);
        if (act == 1 && !c->lines[i].is_shi) score += 1;
        if (act == -1 && !c->lines[i].is_shi) score -= 1;
    }
    return score;
}

// 大白话结论里描述卦象特征的那一句:挑最有信息量的事实说,不堆术语。
// 返回 NULL 表示没有特别值得单说的特征。
static const char *feature_sentence(const liuyao_result_t *r) {
    const liuyao_chart_t *c = &r->chart;
    const liuyao_analysis_t *an = &r->analysis;
    int pos = primary_candidate(an);

    // 优先说卦的大格局:六冲主散、六合主合,是全局性的。
    if (c->pattern == LIUYAO_PATTERN_SIX_CLASH &&
        c->changed_pattern == LIUYAO_PATTERN_SIX_HARMONY) {
        return "这卦是“先散后聚”,开头闹心,后面能拢回来,别在开头就泄气。";
    }
    if (c->pattern == LIUYAO_PATTERN_SIX_HARMONY &&
        c->changed_pattern == LIUYAO_PATTERN_SIX_CLASH) {
        return "这卦是“先合后散”,前面顺,中途要防生变,好事别拖太久。";
    }
    if (c->pattern == LIUYAO_PATTERN_SIX_CLASH) {
        return "这卦逢六冲,主散主快,事情容易反复,定下来的事要抓紧办。";
    }
    if (c->pattern == LIUYAO_PATTERN_SIX_HARMONY) {
        return "这卦逢六合,主合主成,贵在人和,多拉拢能帮你的人。";
    }
    if (pos > 0) {
        const liuyao_line_t *l = &c->lines[pos - 1];
        if (an->candidate_count == 0) {
            return "你关心的这件事,卦里没有明着出现,说明眼下还不到火候,得等机会露头。";
        }
        if (l->is_void) {
            return "这事现在还“空”着,条件没落实,急也没用,等日子到了自然成形。";
        }
        if (l->is_month_break) {
            return "这个月对这件事不利,硬冲容易碰壁,过了这个月再发力。";
        }
        if (l->moving && l->change_advance == 1) {
            return "关键的那一爻在化进神,势头在往上走,可以顺势加一把劲。";
        }
        if (l->moving && l->change_advance == -1) {
            return "关键的那一爻在化退神,后劲不足,见好就收别贪。";
        }
        if (l->strength == LIUYAO_STRENGTH_WEAKENED) {
            return "这卦里帮你的人不多,更多是阻力,这时候稳比冲更重要。";
        }
        if (l->strength == LIUYAO_STRENGTH_SUPPORTED) {
            return "这卦里生扶的力量足,条件是向着你的,可以放手做一些尝试。";
        }
    }
    // 动爻冲克世爻:外力给你施压。
    int shi_branch = c->lines[c->shi_pos - 1].branch_index;
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        if (!c->lines[i].moving || c->lines[i].is_shi) continue;
        if (action_to(&c->lines[i], shi_branch) == -1) {
            return "有动爻在克制你的位置,外部有人或事在压你,先摸清来路再应对。";
        }
    }
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        if (!c->lines[i].moving || c->lines[i].is_shi) continue;
        if (action_to(&c->lines[i], shi_branch) == 1) {
            return "有动爻在生扶你的位置,暗处有人帮你,不用一个人硬扛。";
        }
    }
    return NULL;
}

// 结论页分类白话建议(不用术数词,普通人口吻;下标与 liuyao_category 对齐)。
static const char *const k_plain_advice[LIUYAO_CAT_COUNT] = {
    "不管问什么,先把最要紧的一件事定下来,其他事会跟着顺。",
    "工作上的事,稳住手头的节奏,该争取就去争取,别自己吓自己。",
    "钱的事急不来,看准了再出手,别把本钱放在不踏实的地方。",
    "感情的事多沟通、少猜疑,心意到了,关系自然会缓和。",
    "学习没有捷径,按计划一步步来,考试时放平心态就好。",
    "出发前把行程和证件都核对好,路上稳一点,别赶时间。",
    "家里的事,先把住的地方收拾顺当,家人之间多体谅。",
    "打官司费心费力,能协商解决最好,不行就把证据准备齐全。",
    "家里人之间多说说心里话,小事别计较,气氛好了事就顺。",
};

static void page_conclusion(const liuyao_result_t *r, appender_t *a) {
    const liuyao_analysis_t *an = &r->analysis;
    int score = tendency_score(r);
    int category = an->category;
    if (category < 0 || category >= LIUYAO_CAT_COUNT) category = 0;
    apf(a, "【结论】\n综合来看:%s",
        score >= 2 ? "事情比较顺,条件对你有利,想做的事可以放心去做。\n"
                   : score <= -2
                         ? "眼下不太顺,阻力比较多,先别急着推进,缓一缓、稳一稳更好。\n"
                         : "说不上好也说不上坏,关键看你自己的安排和时机抓得怎么样。\n");
    const char *feature = feature_sentence(r);
    if (feature) apf(a, "%s\n", feature);
    apf(a, "%s\n", k_plain_advice[category]);
    apf(a, "这些说法只是参考,事情最后怎么样,还得看你自己。\n");
}

void liuyao_compose_reading(const liuyao_result_t *result, const char *day_gz,
                            const char *month_gz, liuyao_reading_t *out) {
    if (!result || !out) return;
    memset(out, 0, sizeof(*out));
    appender_t appenders[LIUYAO_READING_PAGE_COUNT];
    for (int i = 0; i < LIUYAO_READING_PAGE_COUNT; i++) {
        appenders[i].buf = out->pages[i];
        appenders[i].size = LIUYAO_READING_PAGE_BYTES;
        appenders[i].used = 0;
        appenders[i].truncated = false;
    }
    page_chart(result, day_gz, month_gz, &appenders[0]);
    page_yongshen(result, &appenders[1]);
    page_moving(result, &appenders[2]);
    page_conclusion(result, &appenders[3]);
    out->page_count = LIUYAO_READING_PAGE_COUNT;
    for (int i = 0; i < LIUYAO_READING_PAGE_COUNT; i++) {
        if (appenders[i].truncated) out->truncated = true;
    }
}
