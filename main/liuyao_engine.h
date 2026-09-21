// main/liuyao_engine.h —— 文王纳甲六爻排盘引擎(纯逻辑,可主机测试,不依赖 IDF/LVGL)。
// 移植自 fortune-liuyao skill 的确定性引擎(liuyao_core.py, wenwang_najia_v1),
// 行为以 tests/liuyao_fixtures.h 的黄金向量锁定。
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LIUYAO_LINE_COUNT 6

// 所问之事(决定用神)。与 UI 选择顺序一致。
typedef enum {
    LIUYAO_CAT_GENERAL = 0,  // 综合
    LIUYAO_CAT_CAREER,       // 事业
    LIUYAO_CAT_WEALTH,       // 求财
    LIUYAO_CAT_RELATION,     // 感情
    LIUYAO_CAT_ACADEMIC,     // 学业
    LIUYAO_CAT_TRAVEL,       // 出行
    LIUYAO_CAT_HOME,         // 家宅
    LIUYAO_CAT_LEGAL,        // 官司
    LIUYAO_CAT_FAMILY,       // 家庭(代占)
    LIUYAO_CAT_COUNT
} liuyao_category_t;

// 感情类取用神视角。
typedef enum {
    LIUYAO_PERSP_UNSPECIFIED = 0,  // 不按性别
    LIUYAO_PERSP_MALE,             // 男问(用神妻财)
    LIUYAO_PERSP_FEMALE,           // 女问(用神官鬼)
} liuyao_perspective_t;

// 月建/日辰对爻的五行动作方向。
typedef enum {
    LIUYAO_REL_SAME = 0,       // 同气(比和)
    LIUYAO_REL_GENERATES,      // 生此爻(月/日生爻)
    LIUYAO_REL_CONTROLS,       // 克此爻(月/日克爻)
    LIUYAO_REL_GENERATED_BY,   // 受生(爻生月/日)
    LIUYAO_REL_CONTROLLED_BY,  // 受制(爻克月/日,泄气)
} liuyao_relation_t;

typedef enum {
    LIUYAO_STRENGTH_NEUTRAL = 0,
    LIUYAO_STRENGTH_SUPPORTED,
    LIUYAO_STRENGTH_WEAKENED,
    LIUYAO_STRENGTH_CONTESTED,
} liuyao_strength_t;

// 卦式:0 普通卦,1 六冲卦,2 六合卦。
#define LIUYAO_PATTERN_ORDINARY 0
#define LIUYAO_PATTERN_SIX_CLASH 1
#define LIUYAO_PATTERN_SIX_HARMONY 2

typedef struct {
    int value;        // 6..9(老阴/少阳/少阴/老阳),自下而上
    bool moving;      // 动爻(6/9)
    bool yang;        // 本爻阴阳
    const char *stem;
    const char *branch;
    const char *element;
    const char *relative;  // 六亲
    const char *spirit;    // 六神
    int branch_index;      // 地支下标(0..11),便于五行运算
    bool is_void;          // 旬空
    bool is_month_break;   // 月破
    bool is_day_clash;     // 日冲(暗动判定不在本引擎)
    bool is_shi;
    bool is_ying;
    liuyao_relation_t month_relation;
    liuyao_relation_t day_relation;
    liuyao_strength_t strength;  // 月日综合旺衰
    bool has_change;
    bool change_yang;
    const char *change_branch;
    const char *change_element;
    const char *change_relative;
    int change_branch_index;  // 变爻地支下标
    int change_advance;   // 1 化进, -1 化退, 0 其他
    bool change_is_void;  // 变爻旬空(化空)
} liuyao_line_t;

typedef struct {
    int position;  // 1..6
    const char *stem;
    const char *branch;
    const char *element;
    const char *relative;
    int branch_index;
    bool is_void;
    bool is_month_break;
    bool is_day_clash;
    const char *flying_branch;  // 同位飞神地支
} liuyao_hidden_t;

typedef struct {
    const char *name;        // 本卦名(如 乾为天)
    const char *upper_name;  // 上卦(外卦)
    const char *lower_name;  // 下卦(内卦)
    const char *upper_element;
    const char *lower_element;
    const char *palace;         // 八宫卦(本宫纯卦卦名字)
    const char *palace_element;
    const char *palace_stage;   // 本宫/一世..五世/游魂/归魂
    int shi_pos;   // 1..6
    int ying_pos;  // 1..6
    const char *changed_name;
    int pattern;          // 本卦卦式
    int changed_pattern;  // 变卦卦式
    int void_branches[2];  // 旬空两支(0..11)
    liuyao_line_t lines[LIUYAO_LINE_COUNT];  // 下标 0 = 初爻
    int hidden_count;
    liuyao_hidden_t hidden[LIUYAO_LINE_COUNT];  // 伏神(按爻位)
} liuyao_chart_t;

typedef struct {
    liuyao_category_t category;
    liuyao_perspective_t perspective;
    const char *yongshen;  // 用神六亲;综合/不按性别家庭为 NULL
    int candidate_count;         // 明露用神爻位数
    int hidden_candidate_count;  // 伏藏用神爻位数
    int candidates[LIUYAO_LINE_COUNT];         // 明露用神爻位(1..6)
    int hidden_candidates[LIUYAO_LINE_COUNT];  // 伏神爻位(1..6)
} liuyao_analysis_t;

typedef struct {
    liuyao_chart_t chart;
    liuyao_analysis_t analysis;
} liuyao_result_t;

typedef struct {
    int lines[LIUYAO_LINE_COUNT];  // 6..9,下标 0 = 初爻
    int day_index;                 // 日柱六十甲子(0=甲子),由历法模块得出
    int month_branch;              // 月支(0=子..11=亥)
    liuyao_category_t category;
    liuyao_perspective_t perspective;
} liuyao_casting_t;

// 排盘。输入非法(爻值/日柱/月支越界)返回 false 且不动 out。
bool liuyao_cast(const liuyao_casting_t *casting, liuyao_result_t *out);

// 干支/五行名称辅助(UI 显示用)。index 越界返回 NULL。
const char *liuyao_stem_str(int index);
const char *liuyao_branch_str(int index);
const char *liuyao_element_str(int element);
// 地支对应五行下标(0木 1火 2土 3金 4水);越界返回 -1。
int liuyao_branch_element(int branch);

#ifdef __cplusplus
}
#endif
