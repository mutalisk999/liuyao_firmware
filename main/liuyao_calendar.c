// main/liuyao_calendar.c —— 六爻干支历法实现。
// 节气表由 tools/gen_liuyao_fixtures.py 同源的 lunar_python 1.4.8 数据生成,
// 主机测试 tests/test_liuyao_calendar.c 用 1424 个采样点对拍。
#include "liuyao_calendar.h"

#define CAL_FIRST_YEAR (LIUYAO_DATE_MIN_YEAR - 1)  // 表首年(2020 年 1 月需要 2019 年的大雪)
#define CAL_LAST_YEAR  LIUYAO_DATE_MAX_YEAR
#define CAL_YEAR_COUNT (CAL_LAST_YEAR - CAL_FIRST_YEAR + 1)

// 12 个节(月柱分界)的精确时刻,按当年公历月排列(1月小寒..12月大雪)。
// 值 = (日-1)*1440 + 当日分钟,由 lunar_python 1.4.8 的节气表生成(含分钟)。
// 月支从立春起为寅,大雪起为子,小寒起为丑。
static const int k_term_offset[CAL_YEAR_COUNT][12] = {
    {7178, 4994, 7509, 6351, 7382, 7626, 9680, 10273, 10456, 11405, 10164, 9738},
    {7530, 5343, 6416, 5258, 6291, 6538, 8594, 9186, 9368, 10315, 9073, 8649},
    {6443, 4258, 6773, 5615, 6647, 6892, 8945, 9533, 9712, 10659, 9418, 8997},
    {6794, 4610, 7123, 5960, 6985, 7225, 9278, 9869, 10052, 11002, 9765, 9346},
    {7144, 4962, 7476, 6313, 7338, 7578, 9630, 10222, 10406, 11355, 10115, 9692},
    {7489, 5307, 6382, 5222, 6250, 6489, 8540, 9129, 9311, 10259, 9020, 8597},
    {6392, 4210, 6727, 5568, 6597, 6836, 8884, 9471, 9651, 10601, 9364, 8944},
    {6743, 4562, 7079, 5920, 6948, 7188, 9236, 9822, 10001, 10949, 9712, 9292},
    {7089, 4906, 7419, 6257, 7285, 7525, 9577, 10166, 10348, 11297, 10058, 9637},
    {7434, 5251, 6324, 5163, 6192, 6436, 8490, 9081, 9262, 10208, 8967, 8544},
    {6341, 4160, 6677, 5518, 6547, 6789, 8842, 9431, 9611, 10558, 9316, 8893},
    {6690, 4508, 7023, 5861, 6886, 7124, 9175, 9767, 9952, 10905, 9668, 9247},
    {7043, 4858, 7371, 6208, 7235, 7475, 9528, 10122, 10310, 11262, 10025, 9602},
    {7396, 5208, 6280, 5117, 6145, 6387, 8440, 9032, 9217, 10170, 8934, 8513},
    {6308, 4121, 6632, 5468, 6493, 6733, 8784, 9375, 9560, 10513, 9281, 8864},
    {6664, 4481, 6992, 5826, 6849, 7086, 9137, 9729, 9913, 10867, 9633, 9216},
    {7015, 4831, 7341, 6173, 7194, 7430, 9481, 10074, 10262, 11217, 9983, 9565},
    {7363, 5179, 6251, 5086, 6109, 6347, 8397, 8988, 9175, 10129, 8894, 8476},
    {6274, 4091, 6606, 5444, 6469, 6706, 8755, 9343, 9525, 10477, 9244, 8827},
    {6626, 4443, 6955, 5789, 6811, 7045, 9092, 9681, 9866, 10821, 9590, 9176},
    {6976, 4792, 7303, 6135, 7158, 7395, 9446, 10038, 10224, 11177, 9942, 9525},
    {7323, 5139, 6211, 5045, 6069, 6308, 8359, 8950, 9134, 10085, 8849, 8430},
};

static bool is_leap(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int liuyao_days_in_month(int year, int month) {
    static const int k_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && is_leap(year)) return 29;
    return k_days[month - 1];
}

bool liuyao_date_supported(int year, int month, int day) {
    if (year < CAL_FIRST_YEAR + 1 || year > CAL_LAST_YEAR) return false;
    if (month < 1 || month > 12) return false;
    int dim = liuyao_days_in_month(year, month);
    return day >= 1 && day <= dim;
}

// 自 1970-01-01 起的天数(Howard Hinnant 的 days_from_civil 算法)。
static long days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned long yoe = (unsigned long)(y - era * 400);
    unsigned long doy = (unsigned long)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097L + (long)doe - 719468L;
}

// (year, month) 月内的节分界时刻偏移:(日-1)*1440 + 分钟;越界返回 -1。
static long term_offset(int year, int month) {
    if (year < CAL_FIRST_YEAR || year > CAL_LAST_YEAR || month < 1 || month > 12) {
        return -1;
    }
    return k_term_offset[year - CAL_FIRST_YEAR][month - 1];
}

bool liuyao_date_facts(int year, int month, int day, int hour,
                       liuyao_date_facts_t *out) {
    if (hour < 0 || hour > 23) return false;
    if (!liuyao_date_supported(year, month, day)) return false;
    if (!out) return false;

    // 日柱:23 点之后按晚子时归次日。
    int roll = hour >= 23 ? 1 : 0;
    long days = days_from_civil(year, month, day) + roll;
    out->day_index = (int)((days + 17) % 60 + 60) % 60;

    // 月柱:时刻(分钟粒度)之前最近的节分界。按时间顺序排:
    // 去年12月大雪 → 今年1月小寒 → ... → 今年12月大雪。
    // 月柱不随晚子时滚动(只有日柱滚动),比较用原始时刻。
    static const int k_term_month[13] = {12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    static const int k_term_branch[13] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0};
    long query = days_from_civil(year, month, day) * 1440L + hour * 60;
    long lichun = -1;
    int branch = 0;
    for (int i = 0; i < 13; i++) {
        int y = year - (i == 0 ? 1 : 0);
        long off = term_offset(y, k_term_month[i]);
        if (off < 0) return false;
        if (days_from_civil(y, k_term_month[i], 1) * 1440L + off <= query) {
            branch = k_term_branch[i];
            if (i == 2) lichun = days_from_civil(y, k_term_month[i], 1) * 1440L + off;
        }
    }
    out->month_branch = branch;

    // 年柱:以立春时刻为界(数据与月柱同一张表)。
    if (lichun < 0) {
        long off = term_offset(year, 2);
        if (off < 0) return false;
        lichun = days_from_civil(year, 2, 1) * 1440L + off;
    }
    int pillar_year = query >= lichun ? year : year - 1;
    out->year_stem = ((pillar_year - 4) % 10 + 10) % 10;
    out->year_branch = ((pillar_year - 4) % 12 + 12) % 12;

    // 月干:五虎遁,年上起月,寅月序 0。
    int month_number = (branch - 2 + 12) % 12;  // 寅=0..丑=11
    out->month_stem = (out->year_stem * 2 + 2 + month_number) % 10;
    return true;
}
