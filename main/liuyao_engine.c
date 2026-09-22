// main/liuyao_engine.c —— 文王纳甲排盘引擎实现。
// 全部为查表与整数运算;中文字串为 UTF-8 常量,统一在 k_* 表中定义。
#include "liuyao_engine.h"

#include <string.h>

// 天干/地支/五行下标:干 甲=0..癸=9;支 子=0..亥=11;五行 木=0 火=1 土=2 金=3 水=4。
static const char *const k_stems[10] = {
    "甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸",
};
static const char *const k_branches[12] = {
    "子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥",
};
static const char *const k_elements[5] = {"木", "火", "土", "金", "水"};
// 相生:木→火→土→金→水→木;相克:木克土,火克金,土克水,金克木,水克火。
static const char *const k_relatives[5] = {"兄弟", "父母", "子孙", "官鬼", "妻财"};
static const char *const k_spirits[6] = {"青龙", "朱雀", "勾陈", "腾蛇", "白虎", "玄武"};
static const char *const k_stages[8] = {
    "本宫", "一世", "二世", "三世", "四世", "五世", "游魂", "归魂",
};

// 日干 → 六神起点:甲乙青龙 丙丁朱雀 戊勾陈 己腾蛇 庚辛白虎 壬癸玄武。
static const int k_spirit_start[10] = {0, 0, 1, 1, 2, 3, 4, 4, 5, 5};

// 每支的五行;顺序同 k_branches。子亥=水,寅卯=木,巳午=火,申酉=金,辰戌丑未=土。
static const int k_branch_element[12] = {
    4, 2, 0, 0, 2, 1, 1, 2, 3, 3, 2, 4,
};

// 八经卦:3 位码 bit0=初爻,bit1=二爻,bit2=三爻(1=阳)。
// 名与宫五行:(乾金)(兑金)(离火)(震木)(巽木)(坎水)(艮土)(坤土)。
typedef struct {
    const char *name;
    int element;
} trigram_t;
static const trigram_t k_trigrams[8] = {
    [0] = {"坤", 2}, [1] = {"震", 0}, [2] = {"坎", 4}, [3] = {"兑", 3},
    [4] = {"艮", 2}, [5] = {"离", 1}, [6] = {"巽", 0}, [7] = {"乾", 3},
};

// 64 卦名:k_hex_names[上卦码][下卦码]。下卦序按乾兑离震巽坎艮坤 = 码 7,3,5,1,6,2,4,0。
#define G_QIAN 7
#define G_DUI 3
#define G_LI 5
#define G_ZHEN 1
#define G_XUN 6
#define G_KAN 2
#define G_GEN 4
#define G_KUN 0
static const char *const k_hex_names[8][8] = {
    // 上卦 坤
    [G_KUN] = {
        [G_KUN] = "坤为地", [G_ZHEN] = "地雷复", [G_KAN] = "地水师", [G_DUI] = "地泽临",
        [G_GEN] = "地山谦", [G_LI] = "地火明夷", [G_XUN] = "地风升", [G_QIAN] = "地天泰",
    },
    // 上卦 震
    [G_ZHEN] = {
        [G_KUN] = "雷地豫", [G_ZHEN] = "震为雷", [G_KAN] = "雷水解", [G_DUI] = "雷泽归妹",
        [G_GEN] = "雷山小过", [G_LI] = "雷火丰", [G_XUN] = "雷风恒", [G_QIAN] = "雷天大壮",
    },
    // 上卦 坎
    [G_KAN] = {
        [G_KUN] = "水地比", [G_ZHEN] = "水雷屯", [G_KAN] = "坎为水", [G_DUI] = "水泽节",
        [G_GEN] = "水山蹇", [G_LI] = "水火既济", [G_XUN] = "水风井", [G_QIAN] = "水天需",
    },
    // 上卦 兑
    [G_DUI] = {
        [G_KUN] = "泽地萃", [G_ZHEN] = "泽雷随", [G_KAN] = "泽水困", [G_DUI] = "兑为泽",
        [G_GEN] = "泽山咸", [G_LI] = "泽火革", [G_XUN] = "泽风大过", [G_QIAN] = "泽天夬",
    },
    // 上卦 艮
    [G_GEN] = {
        [G_KUN] = "山地剥", [G_ZHEN] = "山雷颐", [G_KAN] = "山水蒙", [G_DUI] = "山泽损",
        [G_GEN] = "艮为山", [G_LI] = "山火贲", [G_XUN] = "山风蛊", [G_QIAN] = "山天大畜",
    },
    // 上卦 离
    [G_LI] = {
        [G_KUN] = "火地晋", [G_ZHEN] = "火雷噬嗑", [G_KAN] = "火水未济", [G_DUI] = "火泽睽",
        [G_GEN] = "火山旅", [G_LI] = "离为火", [G_XUN] = "火风鼎", [G_QIAN] = "火天大有",
    },
    // 上卦 巽
    [G_XUN] = {
        [G_KUN] = "风地观", [G_ZHEN] = "风雷益", [G_KAN] = "风水涣", [G_DUI] = "风泽中孚",
        [G_GEN] = "风山渐", [G_LI] = "风火家人", [G_XUN] = "巽为风", [G_QIAN] = "风天小畜",
    },
    // 上卦 乾
    [G_QIAN] = {
        [G_KUN] = "天地否", [G_ZHEN] = "天雷无妄", [G_KAN] = "天水讼", [G_DUI] = "天泽履",
        [G_GEN] = "天山遁", [G_LI] = "天火同人", [G_XUN] = "天风姤", [G_QIAN] = "乾为天",
    },
};

// 纳甲:表项为 {下卦干, 下卦三支}{上卦干, 上卦三支},支按下标 0..2 对应
// 该三画的初/二/三爻。支存 0..11 下标。
typedef struct {
    int stem;
    int branches[3];
} najia_side_t;
typedef struct {
    najia_side_t lower;
    najia_side_t upper;
} najia_t;
static const najia_t k_najia[8] = {
    [G_QIAN] = {{0, {0, 2, 4}}, {8, {6, 8, 10}}},
    [G_KUN]  = {{1, {7, 5, 3}}, {9, {1, 11, 9}}},
    [G_ZHEN] = {{6, {0, 2, 4}}, {6, {6, 8, 10}}},
    [G_XUN]  = {{7, {1, 11, 9}}, {7, {7, 5, 3}}},
    [G_KAN]  = {{4, {2, 4, 6}}, {4, {8, 10, 0}}},
    [G_LI]   = {{5, {3, 1, 11}}, {5, {9, 7, 5}}},
    [G_GEN]  = {{2, {4, 6, 8}}, {2, {10, 0, 2}}},
    [G_DUI]  = {{3, {5, 3, 1}}, {3, {11, 9, 7}}},
};

// 京房八宫:与某纯卦的差异位掩码(bit0=初爻) → {宫名, 世爻位}。
typedef struct {
    int mask;
    int stage;  // k_stages 下标
    int shi;    // 1..6
} palace_pattern_t;
static const palace_pattern_t k_palace_patterns[] = {
    {0, 0, 6},        // 本宫
    {0x01, 1, 1},     // 一世
    {0x03, 2, 2},     // 二世
    {0x07, 3, 3},     // 三世
    {0x0F, 4, 4},     // 四世
    {0x1F, 5, 5},     // 五世
    {0x17, 6, 4},     // 游魂
    {0x10, 7, 3},     // 归魂
};

// 六冲/六合卦集合。
static const char *const k_six_clash[] = {
    "乾为天", "兑为泽", "离为火", "震为雷", "巽为风", "坎为水",
    "艮为山", "坤为地", "天雷无妄", "雷天大壮",
};
static const char *const k_six_harmony[] = {
    "天地否", "地天泰", "水泽节", "泽水困", "山火贲",
    "火山旅", "雷地豫", "地雷复",
};

// 化进神:亥→子 寅→卯 巳→午 申→酉 丑→辰 辰→未 未→戌 戌→丑;其余 -1。
static const int k_advance[12] = {
    -1, 4, 3, -1, 7, 6, -1, 10, 9, -1, 1, 0,
};

static int trigram_code(int b0, int b1, int b2) {
    return b0 | (b1 << 1) | (b2 << 2);
}

static int pattern_of(const char *name) {
    for (size_t i = 0; i < sizeof(k_six_clash) / sizeof(k_six_clash[0]); i++) {
        if (strcmp(k_six_clash[i], name) == 0) return LIUYAO_PATTERN_SIX_CLASH;
    }
    for (size_t i = 0; i < sizeof(k_six_harmony) / sizeof(k_six_harmony[0]); i++) {
        if (strcmp(k_six_harmony[i], name) == 0) return LIUYAO_PATTERN_SIX_HARMONY;
    }
    return LIUYAO_PATTERN_ORDINARY;
}

// 月/日( actor )对该爻( target )的作用。
static liuyao_relation_t relation_of(int actor_element, int target_element) {
    if (actor_element == target_element) return LIUYAO_REL_SAME;
    if ((actor_element + 1) % 5 == target_element) return LIUYAO_REL_GENERATES;
    if ((actor_element + 2) % 5 == target_element) return LIUYAO_REL_CONTROLS;
    if ((target_element + 1) % 5 == actor_element) return LIUYAO_REL_GENERATED_BY;
    return LIUYAO_REL_CONTROLLED_BY;
}

// 六亲:以宫五行为我。
static const char *relative_of(int palace_element, int line_element) {
    if (line_element == palace_element) return k_relatives[0];          // 兄弟
    if ((line_element + 1) % 5 == palace_element) return k_relatives[1]; // 父母(生我)
    if ((palace_element + 1) % 5 == line_element) return k_relatives[2]; // 子孙(我生)
    if ((line_element + 2) % 5 == palace_element) return k_relatives[3]; // 官鬼(克我)
    return k_relatives[4];                                               // 妻财(我克)
}

static int najia_stem(int trigram, int position) {
    return position <= 3 ? k_najia[trigram].lower.stem : k_najia[trigram].upper.stem;
}

static int najia_branch(int trigram, int position) {
    return position <= 3 ? k_najia[trigram].lower.branches[position - 1]
                         : k_najia[trigram].upper.branches[position - 4];
}

const char *liuyao_stem_str(int index) {
    return index >= 0 && index < 10 ? k_stems[index] : NULL;
}

const char *liuyao_branch_str(int index) {
    return index >= 0 && index < 12 ? k_branches[index] : NULL;
}

int liuyao_branch_element(int branch) {
    return branch >= 0 && branch < 12 ? k_branch_element[branch] : -1;
}

bool liuyao_cast(const liuyao_casting_t *casting, liuyao_result_t *out) {    if (!casting || !out) return false;
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        if (casting->lines[i] < 6 || casting->lines[i] > 9) return false;
    }
    if (casting->day_index < 0 || casting->day_index >= 60) return false;
    if (casting->month_branch < 0 || casting->month_branch >= 12) return false;
    if (casting->category < 0 || casting->category >= LIUYAO_CAT_COUNT) return false;

    int day_stem = casting->day_index % 10;
    int day_branch = casting->day_index % 12;
    int month_branch = casting->month_branch;

    int bits[LIUYAO_LINE_COUNT];
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        bits[i] = (casting->lines[i] == 7 || casting->lines[i] == 9) ? 1 : 0;
    }
    int lower = trigram_code(bits[0], bits[1], bits[2]);
    int upper = trigram_code(bits[3], bits[4], bits[5]);
    liuyao_chart_t *c = &out->chart;
    memset(c, 0, sizeof(*c));
    c->lower_name = k_trigrams[lower].name;
    c->lower_element = k_elements[k_trigrams[lower].element];
    c->upper_name = k_trigrams[upper].name;
    c->upper_element = k_elements[k_trigrams[upper].element];
    c->name = k_hex_names[upper][lower];
    c->pattern = pattern_of(c->name);

    // 变卦。
    int changed_bits[LIUYAO_LINE_COUNT];
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        changed_bits[i] = bits[i];
        if (casting->lines[i] == 6 || casting->lines[i] == 9) {
            changed_bits[i] ^= 1;
        }
    }
    int chg_lower = trigram_code(changed_bits[0], changed_bits[1], changed_bits[2]);
    int chg_upper = trigram_code(changed_bits[3], changed_bits[4], changed_bits[5]);
    c->changed_name = k_hex_names[chg_upper][chg_lower];
    c->changed_pattern = pattern_of(c->changed_name);

    // 八宫定世应:与某纯卦的差异掩码命中八宫格局即得宫、卦世与世爻。
    int palace = -1;
    int stage = 0;
    int shi = 0;
    for (int t = 0; t < 8 && palace < 0; t++) {
        int pure_bits[3] = {t & 1, (t >> 1) & 1, (t >> 2) & 1};
        int mask = 0;
        for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
            if (bits[i] != pure_bits[i % 3]) mask |= (1 << i);
        }
        for (size_t p = 0; p < sizeof(k_palace_patterns) / sizeof(k_palace_patterns[0]);
             p++) {
            if (k_palace_patterns[p].mask == mask) {
                palace = t;
                stage = k_palace_patterns[p].stage;
                shi = k_palace_patterns[p].shi;
                break;
            }
        }
    }
    if (palace < 0) return false;
    int palace_element = k_trigrams[palace].element;
    c->palace = k_trigrams[palace].name;
    c->palace_element = k_elements[palace_element];
    c->palace_stage = k_stages[stage];
    c->shi_pos = shi;
    c->ying_pos = (shi + 2) % 6 + 1;

    // 旬空。
    int void_start = (day_branch - day_stem + 12) % 12;
    c->void_branches[0] = (void_start + 10) % 12;
    c->void_branches[1] = (void_start + 11) % 12;

    // 逐爻装卦。
    int month_element = k_branch_element[month_branch];
    int day_element = k_branch_element[day_branch];
    bool support = false;
    bool pressure = false;
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        liuyao_line_t *line = &c->lines[i];
        int position = i + 1;
        int trigram = position <= 3 ? lower : upper;
        int branch = najia_branch(trigram, position);
        int element = k_branch_element[branch];

        line->value = casting->lines[i];
        line->moving = line->value == 6 || line->value == 9;
        line->yang = bits[i] != 0;
        line->stem = k_stems[najia_stem(trigram, position)];
        line->branch = k_branches[branch];
        line->element = k_elements[element];
        line->relative = relative_of(palace_element, element);
        line->spirit = k_spirits[(k_spirit_start[day_stem] + i) % 6];
        line->branch_index = branch;
        line->is_void = branch == c->void_branches[0] || branch == c->void_branches[1];
        line->is_month_break = branch == (month_branch + 6) % 12;
        line->is_day_clash = branch == (day_branch + 6) % 12;
        line->is_shi = position == shi;
        line->is_ying = position == c->ying_pos;
        line->month_relation = relation_of(month_element, element);
        line->day_relation = relation_of(day_element, element);
        support = false;
        pressure = false;
        if (line->month_relation == LIUYAO_REL_SAME ||
            line->month_relation == LIUYAO_REL_GENERATES) {
            support = true;
        } else if (line->month_relation == LIUYAO_REL_CONTROLS) {
            pressure = true;
        }
        if (line->day_relation == LIUYAO_REL_SAME ||
            line->day_relation == LIUYAO_REL_GENERATES) {
            support = true;
        } else if (line->day_relation == LIUYAO_REL_CONTROLS) {
            pressure = true;
        }
        line->strength = support && pressure ? LIUYAO_STRENGTH_CONTESTED
                         : support           ? LIUYAO_STRENGTH_SUPPORTED
                         : pressure          ? LIUYAO_STRENGTH_WEAKENED
                                             : LIUYAO_STRENGTH_NEUTRAL;

        if (line->moving) {
            int chg_trigram = position <= 3 ? chg_lower : chg_upper;
            int chg_branch = najia_branch(chg_trigram, position);
            int chg_element = k_branch_element[chg_branch];
            line->has_change = true;
            line->change_yang = changed_bits[i] != 0;
            line->change_branch = k_branches[chg_branch];
            line->change_element = k_elements[chg_element];
            line->change_relative = relative_of(palace_element, chg_element);
            line->change_branch_index = chg_branch;
            line->change_advance = k_advance[branch] == chg_branch ? 1
                                   : k_advance[chg_branch] == branch ? -1 : 0;
            line->change_is_void = chg_branch == c->void_branches[0] ||
                                   chg_branch == c->void_branches[1];
        }
    }

    // 伏神:宫位纯卦纳甲中,卦中不见的六亲。
    bool seen[5] = {false, false, false, false, false};
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        for (int e = 0; e < 5; e++) {
            if (c->lines[i].relative == k_relatives[e]) seen[e] = true;
        }
    }
    c->hidden_count = 0;
    for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
        int position = i + 1;
        int trigram = palace;  // 纯卦上下同卦
        int branch = najia_branch(trigram, position);
        int element = k_branch_element[branch];
        const char *relative = relative_of(palace_element, element);
        bool visible = false;
        for (int e = 0; e < 5; e++) {
            if (relative == k_relatives[e] && seen[e]) visible = true;
        }
        if (visible) continue;
        liuyao_hidden_t *h = &c->hidden[c->hidden_count++];
        h->position = position;
        h->stem = k_stems[najia_stem(trigram, position)];
        h->branch = k_branches[branch];
        h->element = k_elements[element];
        h->relative = relative;
        h->branch_index = branch;
        h->is_void = branch == c->void_branches[0] || branch == c->void_branches[1];
        h->is_month_break = branch == (month_branch + 6) % 12;
        h->is_day_clash = branch == (day_branch + 6) % 12;
        h->flying_branch = c->lines[i].branch;
    }

    // 用神。
    liuyao_analysis_t *a = &out->analysis;
    memset(a, 0, sizeof(*a));
    a->category = casting->category;
    a->perspective = casting->perspective;
    switch (casting->category) {
        case LIUYAO_CAT_CAREER:
        case LIUYAO_CAT_LEGAL:
            a->yongshen = k_relatives[3];
            break;
        case LIUYAO_CAT_WEALTH:
            a->yongshen = k_relatives[4];
            break;
        case LIUYAO_CAT_ACADEMIC:
        case LIUYAO_CAT_HOME:
            a->yongshen = k_relatives[1];
            break;
        case LIUYAO_CAT_RELATION:
            if (casting->perspective == LIUYAO_PERSP_FEMALE) {
                a->yongshen = k_relatives[3];
            } else if (casting->perspective == LIUYAO_PERSP_MALE) {
                a->yongshen = k_relatives[4];
            }
            break;
        default:
            a->yongshen = NULL;
            break;
    }
    if (casting->category == LIUYAO_CAT_TRAVEL) {
        // 出行以世爻为用。
        static const char world_line[] = "世爻";
        a->yongshen = world_line;
        a->candidates[a->candidate_count++] = shi;
    } else if (a->yongshen != NULL) {
        for (int i = 0; i < LIUYAO_LINE_COUNT; i++) {
            if (c->lines[i].relative == a->yongshen) {
                a->candidates[a->candidate_count++] = i + 1;
            }
        }
        for (int i = 0; i < c->hidden_count; i++) {
            if (c->hidden[i].relative == a->yongshen) {
                a->hidden_candidates[a->hidden_candidate_count++] =
                    c->hidden[i].position;
            }
        }
    }
    return true;
}
