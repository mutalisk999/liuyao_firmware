// main/liuyao_data.c —— 六爻解读静态文案数据。
// 卦意取传统卦名卦义的白话短释,顺序与 liuyao_engine.c 的 64 卦名一致,
// 便于核对齐全性(主机测试会对全部 64 卦逐一查表)。
#include "liuyao_data.h"

#include <stddef.h>
#include <string.h>

#include "liuyao_engine.h"

typedef struct {
    const char *name;
    const char *keyword;
    const char *meaning;
} hex_meaning_t;

// 顺序与 k_hex_names(上卦坤震坎兑艮离巽乾 × 下卦同序)一致。
static const hex_meaning_t k_hex_meanings[64] = {
    {"坤为地", "厚德", "厚德载物，宜守成包容，静待时机"},
    {"地雷复", "复归", "一阳来复，否极泰来，宜渐进勿急"},
    {"地水师", "兴师", "竞争激烈，宜有统筹有纪律，行事守正"},
    {"地泽临", "临下", "阳长之势，宜把握时机，虚心待人"},
    {"地山谦", "谦逊", "谦受益满招损，卑以自守，百事顺遂"},
    {"地火明夷", "晦暗", "光明受掩，宜藏智守拙，避害自保"},
    {"地风升", "渐进", "积小成高，顺势上升，宜循序求进"},
    {"地天泰", "通泰", "上下相通，天地交泰，诸事顺遂"},
    {"雷地豫", "悦豫", "和乐预备，宜顺势而行，防乐极生悲"},
    {"震为雷", "震动", "雷声震动，临危不乱，惊惧之后自安"},
    {"雷水解", "解缓", "险困得解，宜速离险地，宽宥前行"},
    {"雷泽归妹", "归嫁", "礼有未备，宜守分谨慎，勿强求冒进"},
    {"雷山小过", "小过", "可小事不可大事，宜稍敛就下，低调度过"},
    {"雷火丰", "丰大", "盛大之极，日中则昃，宜守中不骄"},
    {"雷风恒", "恒久", "持之以恒，宜守常道，不可朝令夕改"},
    {"雷天大壮", "大壮", "阳气壮盛，宜正而用壮，戒骄戒躁"},
    {"水地比", "亲比", "宜择善依附，亲近贵人，互助成事"},
    {"水雷屯", "始难", "万事起头难，宜蓄力待援，不可冒进"},
    {"坎为水", "重险", "险阻重重，宜守常持信，行险而不失其信"},
    {"水泽节", "节制", "制度有节，宜节俭自守，适可而止"},
    {"水山蹇", "蹇难", "前有险阻，行路蹇滞，宜反身修德待时"},
    {"水火既济", "完成", "功成初定，防初吉终乱，守成不易"},
    {"水风井", "井泉", "养人不穷，宜守常安分，修己待用"},
    {"水天需", "待时", "时机未到，宜从容等待，蓄养实力"},
    {"泽地萃", "聚合", "人聚物聚，宜广结善缘，防意外之变"},
    {"泽雷随", "随从", "随时而动，宜顺从正道，不可盲随"},
    {"泽水困", "困顿", "处境困窘，宜安命自守，言不信时姑默"},
    {"兑为泽", "和悦", "朋友讲习，和颜悦色，宜以诚待人"},
    {"泽山咸", "感应", "二气感应，人际和合，宜以至诚相与"},
    {"泽火革", "变革", "顺天应人，时机成熟方可革故鼎新"},
    {"泽风大过", "大过", "非常之时，负担过重，宜量力而行"},
    {"泽天夬", "决断", "宜决而能和，明示号令，戒惕慎行"},
    {"山地剥", "剥落", "阴盛剥阳，诸事剥蚀，宜顺时止步守身"},
    {"山雷颐", "颐养", "慎言节食，宜自食其力，养正则吉"},
    {"山水蒙", "启蒙", "蒙昧待启，宜请教明师，认清方向"},
    {"山泽损", "损益", "先损后益，宜减损妄念浮费，实事求事"},
    {"艮为山", "知止", "时止则止，宜适可而止，各安其位"},
    {"山火贲", "文饰", "修饰外表，宜质文相称，小事可行"},
    {"山风蛊", "整饬", "积弊待治，宜整顿革新，防内部隐患"},
    {"山天大畜", "大畜", "蓄德蓄能，宜止而蓄，厚积薄发"},
    {"火地晋", "晋进", "如日出地，光明上进，宜柔顺前行"},
    {"火雷噬嗑", "明断", "遇阻须排，宜明断是非，刚柔相济"},
    {"火水未济", "未竟", "事未完成，慎终如始，未来大有希望"},
    {"火泽睽", "背异", "乖异离心，同床异梦，宜求同存异"},
    {"火山旅", "旅次", "旅途漂泊，宜谨慎柔顺，小事可通"},
    {"离为火", "附丽", "光明相续，宜依附正道，柔顺守中"},
    {"火风鼎", "鼎新", "去故取新，养贤成器，事业可期"},
    {"火天大有", "大有", "所有丰大，名利可收，宜谦逊守成"},
    {"风地观", "观望", "观仰审势，宜静观其变，不宜轻动"},
    {"风雷益", "增益", "损上益下，施惠得众，宜乘势进取"},
    {"风水涣", "涣散", "人心涣散，宜聚拢精神，防各自为政"},
    {"风泽中孚", "诚信", "中心诚信，可感于人，宜笃守承诺"},
    {"风山渐", "渐进", "循序渐进，如鸿渐陆，婚嫁为吉象"},
    {"风火家人", "家人", "治家有道，内外有别，宜先正其身"},
    {"巽为风", "随顺", "谦柔随顺，宜申命行事，不可刚断"},
    {"风天小畜", "畜养", "力量尚小，宜积小成大，暂敛锋芒"},
    {"天地否", "闭塞", "天地不交，诸事阻隔，宜守静待变"},
    {"天雷无妄", "无妄", "守正无妄，顺乎自然，妄动则有灾"},
    {"天水讼", "争讼", "恐有争执，宜和解退让，勿逞强斗胜"},
    {"天泽履", "谨行", "如履虎尾，宜谨慎守礼，方可无咎"},
    {"天山遁", "退避", "宜退不宜进，远小人以自全，静俟时来"},
    {"天火同人", "同人", "志同道合，宜协同行事，光明无私"},
    {"天风姤", "遇合", "不期而遇，防小人近身，勿轻信亲近"},
    {"乾为天", "刚健", "天行刚健，自强不息，大有可为之时"},
};

static const char *const k_category_labels[LIUYAO_CAT_COUNT] = {
    "综合", "事业", "求财", "感情", "学业", "出行", "家宅", "官司", "家庭",
};

static const char *const k_category_focus[LIUYAO_CAT_COUNT] = {
    "以世爻为我，兼看用神、月日与动变。",
    "官鬼为用神，世爻看自身承接，父母为文书合同，兄弟为竞争。",
    "妻财为用神，子孙为财源，兄弟为耗散与竞争。",
    "以世应互动为主线，财官为对方之象，结合视角取用。",
    "父母为成绩文书，官鬼为考试结果，世爻看临场发挥。",
    "世爻为行人自身，应爻为目的地与环境，父母为车船凭证。",
    "父母为房屋契约，世爻为居住者，应爻为环境邻舍。",
    "官鬼为程序与压力，父母为证据文书，世应分主客两造。",
    "代占按六亲取用神，世应定彼此角色，先明所代之人。",
};

static const char *const k_perspective_labels[LIUYAO_PERSP_FEMALE + 1] = {
    "不按性别", "男问", "女问",
};

static const char *const k_position_labels[LIUYAO_LINE_COUNT + 1] = {
    NULL, "初爻", "二爻", "三爻", "四爻", "五爻", "上爻",
};

static const char *const k_value_labels[10] = {
    NULL, NULL, NULL, NULL, NULL, NULL, "老阴", "少阳", "少阴", "老阳",
};

const char *liuyao_hexagram_keyword(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < 64; i++) {
        if (k_hex_meanings[i].name == name ||
            strcmp(k_hex_meanings[i].name, name) == 0) {
            return k_hex_meanings[i].keyword;
        }
    }
    return NULL;
}

const char *liuyao_hexagram_meaning(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < 64; i++) {
        if (k_hex_meanings[i].name == name ||
            strcmp(k_hex_meanings[i].name, name) == 0) {
            return k_hex_meanings[i].meaning;
        }
    }
    return NULL;
}

const char *liuyao_category_label(int category) {
    return category >= 0 && category < LIUYAO_CAT_COUNT
               ? k_category_labels[category]
               : NULL;
}

const char *liuyao_category_focus(int category) {
    return category >= 0 && category < LIUYAO_CAT_COUNT
               ? k_category_focus[category]
               : NULL;
}

const char *liuyao_perspective_label(int perspective) {
    return perspective >= 0 && perspective <= LIUYAO_PERSP_FEMALE
               ? k_perspective_labels[perspective]
               : NULL;
}

const char *liuyao_line_position_label(int position) {
    return position >= 1 && position <= LIUYAO_LINE_COUNT
               ? k_position_labels[position]
               : NULL;
}

const char *liuyao_line_value_label(int value) {
    return value >= 6 && value <= 9 ? k_value_labels[value] : NULL;
}
