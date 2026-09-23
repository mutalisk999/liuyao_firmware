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

// 用神的有效数据视图。明露时取该爻;伏藏时必须取伏神自己——伏神的地支、
// 五行、旬空、月破与压在上面的飞神是两回事,用飞神的数据去写"用神"
// 会把整页解读写错(飞神旺不等于伏神旺)。
typedef struct {
    const char *branch;
    const char *element;
    int branch_index;
    bool is_void;
    bool is_month_break;
    bool is_day_clash;
    liuyao_strength_t strength;
    bool moving;
    int change_advance;  // 1 化进, -1 化退(伏神不动,恒为 0)
} yong_view_t;

// 返回 false 表示卦中既无明露也无伏神候选(走世爻或主线支路)。
static bool yongshen_view(const liuyao_result_t *r, yong_view_t *v) {
    const liuyao_analysis_t *an = &r->analysis;
    int pos = primary_candidate(an);
    if (pos <= 0) return false;
    if (an->candidate_count > 0) {
        const liuyao_line_t *l = &r->chart.lines[pos - 1];
        *v = (yong_view_t){l->branch,  l->element,       l->branch_index,
                           l->is_void, l->is_month_break, l->is_day_clash,
                           l->strength, l->moving, l->change_advance};
        return true;
    }
    for (int i = 0; i < r->chart.hidden_count; i++) {
        if (r->chart.hidden[i].position == pos) {
            const liuyao_hidden_t *h = &r->chart.hidden[i];
            *v = (yong_view_t){h->branch,  h->element,       h->branch_index,
                               h->is_void, h->is_month_break, h->is_day_clash,
                               h->strength, false, 0};
            return true;
        }
    }
    return false;
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
            yong_view_t y;
            yongshen_view(r, &y);
            if (hidden) {
                apf(a, "用神不现，伏于%d爻%s之下(%s%s)，未得引拔。\n", pos,
                    c->lines[pos - 1].branch, y.branch, y.element);
            } else {
                apf(a, "明现于%d爻，%s%s%s，%s。\n", pos, y.branch,
                    y.element, c->lines[pos - 1].relative ? c->lines[pos - 1].relative : "",
                    y.moving ? "发动" : "安静");
            }
            if (y.is_void) apf(a, "值旬空，力量暂不落实。\n");
            if (y.is_month_break) apf(a, "逢月破，本月中受损。\n");
            if (!hidden && y.is_day_clash && !y.moving) {
                // 静爻被日冲:旺相者为暗动(虽静有变),休囚者只是被克散。
                if (y.strength == LIUYAO_STRENGTH_SUPPORTED ||
                    y.strength == LIUYAO_STRENGTH_CONTESTED) {
                    apf(a, "被日辰冲，静者暗动。\n");
                } else {
                    apf(a, "被日辰冲，休囚无力，难有作为。\n");
                }
            }
            apf(a, "月建日辰:%s。\n",
                y.strength == LIUYAO_STRENGTH_SUPPORTED
                    ? "生扶用神，旺相有力"
                    : y.strength == LIUYAO_STRENGTH_WEAKENED
                          ? "克抑用神，衰弱受制"
                          : y.strength == LIUYAO_STRENGTH_CONTESTED
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
    // 伏藏时取伏神地支,否则动爻对"用神"的生克会算到飞神头上。
    yong_view_t yong_view;
    bool have_yong = yongshen_view(r, &yong_view);
    int yong_branch = have_yong ? yong_view.branch_index : -1;

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
        // 伏藏时地支取伏神自己的(冲飞神另说),否则取用神爻地支。
        yong_view_t y;
        bool have_yong = yongshen_view(r, &y);
        const char *br = have_yong ? y.branch : c->lines[yong_pos - 1].branch;
        int yb = have_yong ? y.branch_index : c->lines[yong_pos - 1].branch_index;
        const char *clash = liuyao_branch_str((yb + 6) % 12);
        const char *harmony = liuyao_branch_str(k_harmony[yb]);
        bool any = false;
        if (an->candidate_count == 0) {
            apf(a, "用神伏藏，冲去飞神(%s)之日可见端倪。\n",
                c->lines[yong_pos - 1].branch);
            any = true;
        }
        if (have_yong && y.is_void) {
            apf(a, "%s日填实出空，或%s日冲空而应。\n", br, clash);
            any = true;
        }
        if (have_yong && y.is_month_break) {
            apf(a, "%s日实破，或%s日逢合有望。\n", br, harmony);
            any = true;
        }
        if (have_yong && y.moving && !any) {
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
    yong_view_t y;
    bool have_yong = yongshen_view(r, &y);
    if (have_yong) {
        if (y.strength == LIUYAO_STRENGTH_SUPPORTED) score += 3;
        if (y.strength == LIUYAO_STRENGTH_WEAKENED) score -= 3;
        if (y.is_void) score -= 2;
        if (y.is_month_break) score -= 2;
        if (y.moving) score += 1;
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
// 传入判词分数,保证特征句与判词同一方向;返回 NULL 表示没有特别值得单说的。
static const char *feature_sentence(const liuyao_result_t *r, int score) {
    const liuyao_chart_t *c = &r->chart;
    const liuyao_analysis_t *an = &r->analysis;
    int pos = primary_candidate(an);
    bool good = score >= 2;   // 判词”顺”
    bool bad = score <= -2;   // 判词”滞”

    // 优先说卦的大格局:六冲主散、六合主合,是全局性的。
    // 但判词已经明确向好/向差时,格局句必须与判词同向,否则自相矛盾。
    if (c->pattern == LIUYAO_PATTERN_SIX_CLASH &&
        c->changed_pattern == LIUYAO_PATTERN_SIX_HARMONY) {
        return "卦逢六冲化六合,主先散后聚,初虽纷扰,终见和合。";
    }
    if (c->pattern == LIUYAO_PATTERN_SIX_HARMONY &&
        c->changed_pattern == LIUYAO_PATTERN_SIX_CLASH) {
        return "卦逢六合化六冲,主先合后散,须防中道生变。";
    }
    if (!good && c->pattern == LIUYAO_PATTERN_SIX_CLASH) {
        return "卦逢六冲,主散主快,事多反复,已定者宜速行。";
    }
    if (!bad && c->pattern == LIUYAO_PATTERN_SIX_HARMONY) {
        return "卦逢六合,主合主成,贵在人和,宜借助力。";
    }
    // 伏藏时用伏神自己的旺衰/空破,否则特征句说的是飞神而不是用神。
    yong_view_t y;
    if (pos > 0 && yongshen_view(r, &y)) {
        if (an->candidate_count == 0) {
            // 伏藏要分旺衰:伏而旺是"时机未到",伏而弱才是"用神无力"。
            // 不分就会出现判词说顺、特征句叫人等的自相矛盾。
            if (good) {
                return "用神伏藏不现,然伏神旺相,待引拔之时可成。";
            }
            return "用神伏藏不现,眼下时机未至,须待露头之日。";
        }
        if (y.is_void) {
            // 空而旺与空而衰断法不同:旺则出空即发,衰则出空亦虚。
            return good ? "用神值旬空而旺相,待出空之日可发。"
                        : "用神值旬空,力量未实,待出空之日方有应。";
        }
        if (y.is_month_break) {
            // 月破逢旺相,过月实破可成;休囚逢破,方为本月难为。
            return good ? "用神逢月破而得令,过月实破可成。"
                        : "用神逢月破,本月难为,待过月实破方顺。";
        }
        if (!bad && y.moving && y.change_advance == 1) {
            return "用神化进神,其势渐旺,宜顺势而为。";
        }
        if (!good && y.moving && y.change_advance == -1) {
            return "用神化退神,后劲渐衰,宜见好则收。";
        }
        if (!good && y.strength == LIUYAO_STRENGTH_WEAKENED) {
            return "卦中克泄交加,生扶者寡,宜守不宜攻。";
        }
        if (!bad && y.strength == LIUYAO_STRENGTH_SUPPORTED) {
            return "卦中生扶有力,用神得助,可为可进。";
        }
    }
    // 动爻冲克世爻:外力施压。
    int shi_branch = c->lines[c->shi_pos - 1].branch_index;
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        if (!c->lines[i].moving || c->lines[i].is_shi) continue;
        if (!good && action_to(&c->lines[i], shi_branch) == -1) {
            return "有动爻克制世爻,外力相压,宜先察其来路。";
        }
    }
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        if (!c->lines[i].moving || c->lines[i].is_shi) continue;
        if (!bad && action_to(&c->lines[i], shi_branch) == 1) {
            return "有动爻生扶世爻,暗有助力,不必独支。";
        }
    }
    return NULL;
}

// 结论页:综合倾向(趋吉/偏滞/中平)+ 特征句 + 分类主线 + 参考声明。
// 文案保持传统断语风格,不用大白话;分类主线见 liuyao_category_focus()。

static void page_conclusion(const liuyao_result_t *r, appender_t *a) {
    const liuyao_analysis_t *an = &r->analysis;
    int score = tendency_score(r);
    // 综合倾向只作相对之论,不断定吉凶;特征句与判词同向(见 feature_sentence)。
    apf(a, "【结论】\n综合倾向:%s\n",
        score >= 2 ? "趋吉——用神得力，大局向好，宜把握时机推进。"
                   : score <= -2
                         ? "偏滞——阻力偏重，时机未至，宜缓图守正，不宜强求。"
                         : "中平——吉凶相参，成败系于作为与时机，宜稳中求进。");
    const char *feature = feature_sentence(r, score);
    if (feature) apf(a, "%s\n", feature);
    const char *focus = liuyao_category_focus(an->category);
    if (focus) apf(a, "%s\n", focus);
    apf(a, "六爻乃传统术数参考，吉凶在人，行止由己。\n");
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
