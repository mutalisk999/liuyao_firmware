// main/liuyao_data.h —— 六爻解读静态文案(纯数据,可主机测试)。
// 覆盖契约:设备上显示的全部动态中文都出自 main/liuyao_*.c 中的字符串常量,
// 字库子集由这些源文件提取生成(tools/gen_liuyao_font_symbols.sh)。
#pragma once

// 卦名 → 关键词与卦意短释。查询按键名全等匹配,未收录返回 NULL。
const char *liuyao_hexagram_keyword(const char *name);
const char *liuyao_hexagram_meaning(const char *name);

// 所问之事的中文名(LIUYAO_CAT_COUNT 项)与解读侧重一句。
const char *liuyao_category_label(int category);
const char *liuyao_category_focus(int category);

// 感情视角的中文名(LIUYAO_PERSP_FEMALE+1 项)。
const char *liuyao_perspective_label(int perspective);

// 爻位名称:1 初爻..6 上爻;越界返回 NULL。
const char *liuyao_line_position_label(int position);

// 爻值名称:6 老阴 7 少阳 8 少阴 9 老阳;越界返回 NULL。
const char *liuyao_line_value_label(int value);
