#!/usr/bin/env python3
"""Generate Liu Yao host-test fixtures from the reference Python engine.

Dev-time generator, not part of the CI gate. The committed output is
``tests/liuyao_fixtures.h``; regenerate it after changing the reference
engine or the fixture strategy:

    python tools/gen_liuyao_fixtures.py

The reference implementation is the fortune-liuyao skill's standalone
engine (scripts/liuyao_core.py) plus its vendored lunar_python 1.4.8.
Point LIUYAO_SKILL_DIR at a checkout when it is not installed under
~/.zcode/skills/fortune-liuyao.
"""

from __future__ import annotations

import os
import random
import sys
from datetime import date, timedelta
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SKILL_DIR = Path(
    os.environ.get("LIUYAO_SKILL_DIR", Path.home() / ".zcode/skills/fortune-liuyao")
)
sys.path.insert(0, str(SKILL_DIR / "scripts"))
sys.path.insert(0, str(SKILL_DIR / "vendor"))

import liuyao_core  # noqa: E402
from lunar_python import Solar  # noqa: E402

STEMS = "甲乙丙丁戊己庚辛壬癸"
BRANCHES = "子丑寅卯辰巳午未申酉戌亥"
CATEGORIES = ("general", "career", "wealth", "relationship", "academic",
              "travel", "home", "legal_risk", "relationship_family")
PERSPECTIVES = (None, "male", "female")

# Solar years covered by the embedded solar-term table in liuyao_calendar.c.
CAL_FIRST_YEAR = 2020
CAL_LAST_YEAR = 2040

# The 12 "jie" terms that open a month pillar, in pillar order starting 丑月.
JIE_TERMS = ("小寒", "立春", "惊蛰", "清明", "立夏", "芒种",
             "小暑", "立秋", "白露", "寒露", "立冬", "大雪")
# Pillar branch opened by each term above; 小寒 opens 丑, 立春 opens 寅, ...
JIE_BRANCHES = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0)
# Fixed solar month that carries each term above.
JIE_MONTHS = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)


def jie_table(year: int) -> dict[str, date]:
    lunar = Solar.fromYmd(year, 6, 1).getLunar()
    raw = lunar.getJieQiTable()
    out: dict[str, date] = {}
    for name, value in raw.items():
        short = name[1:] if len(name) > 2 else name
        out[short] = date(value.getYear(), value.getMonth(), value.getDay())
    return out


def sexagenary_index(gz: str) -> int:
    stem, branch = STEMS.index(gz[0]), BRANCHES.index(gz[1])
    for i in range(60):
        if i % 10 == stem and i % 12 == branch:
            return i
    raise ValueError(gz)


def calendar_facts(d: date, hour: int) -> dict[str, int]:
    """Reference values for one sample: day/month/year pillars."""
    rolled = d + timedelta(days=1) if hour >= 23 else d
    day_gz = Solar.fromYmdHms(rolled.year, rolled.month, rolled.day, 12, 0, 0) \
        .getLunar().getEightChar().getDay()
    eight = Solar.fromYmdHms(d.year, d.month, d.day, max(hour, 12) % 24, 0, 0) \
        .getLunar().getEightChar()
    month_gz = eight.getMonth()
    year_gz = eight.getYear()
    return {
        "day": sexagenary_index(day_gz),
        "month_branch": BRANCHES.index(month_gz[1]),
        "month_stem": STEMS.index(month_gz[0]),
        "year_stem": STEMS.index(year_gz[0]),
        "year_branch": BRANCHES.index(year_gz[1]),
    }


REL_LETTER = {"same_element": "s", "generates": "g", "controls": "c",
              "generated_by": "G", "controlled_by": "C"}
ST_LETTER = {"supported": "s", "weakened": "w", "contested": "c", "neutral": "n"}


def fingerprint(chart: dict, analysis: dict) -> str:
    """Canonical text both the Python engine and the C engine must produce."""
    c = chart
    parts = [
        f"name={c['originalHexagram']['name']}",
        f"up={c['originalHexagram']['upperTrigram']['name']}",
        f"low={c['originalHexagram']['lowerTrigram']['name']}",
        f"palace={c['palace']}/{c['palaceElement']}/{c['palaceStage']}",
        f"shi={c['shiPosition']}",
        f"ying={c['yingPosition']}",
        f"chg={c['changedHexagram']['name']}",
        f"void={''.join(c['voidBranches'])}",
        f"pat={ {'six_clash': 'c', 'six_harmony': 'h', 'ordinary': 'o'}[c['originalHexagramPattern']] }>"
        f"{ {'six_clash': 'c', 'six_harmony': 'h', 'ordinary': 'o'}[c['changedHexagramPattern']] }",
        f"ys={analysis['yongshenRelative'] or '-'}",
    ]
    for line in c["lines"]:
        changed = line.get("changedLine")
        if changed:
            adv = {"advance": "a", "retreat": "r", "none": "n"}[changed["advanceRetreat"]]
            chg = "{}{}{}{}{}".format(changed["najiaBranch"], changed["najiaElement"],
                                      changed["sixRelative"], adv,
                                      "V" if changed["isVoid"] else "v")
        else:
            chg = "-"
        flags = "{}{}{}{}{}{}{}{}".format(
            "M" if line["moving"] else "m",
            "S" if line["isShi"] else "s",
            "Y" if line["isYing"] else "y",
            "V" if line["isVoid"] else "v",
            "B" if line["isMonthBreak"] else "b",
            "D" if line["isDayClash"] else "d",
            REL_LETTER[line["monthRelation"]],
            REL_LETTER[line["dayRelation"]],
        )
        status = line["strengthEvidence"]["status"]
        parts.append("{}{}{}{}{}|{}{}#{}".format(
            line["najiaStem"], line["najiaBranch"], line["najiaElement"],
            line["sixRelative"], line["sixSpirit"], flags, ST_LETTER[status], chg))
    if c["hiddenLines"]:
        parts.append("hid=" + ",".join(
            "{}{}{}{}".format(h["position"], h["najiaBranch"],
                              h["najiaElement"], h["sixRelative"])
            for h in c["hiddenLines"]))
    else:
        parts.append("hid=-")
    cand = [item["position"] for item in analysis["candidates"]
            if not item.get("hidden")]
    cand_hidden = [item["position"] for item in analysis["candidates"]
                   if item.get("hidden")]
    parts.append("cand={}.{}".format(",".join(map(str, cand)) or "-",
                                     ",".join(map(str, cand_hidden)) or "-"))
    return "|".join(parts)


def main() -> None:
    rng = random.Random(20260921)
    out: list[str] = []
    out.append("// Generated by tools/gen_liuyao_fixtures.py -- do not edit by hand.")
    out.append("// Reference: fortune-liuyao skill engine + vendored lunar_python 1.4.8.")
    out.append("#pragma once")
    out.append("")
    out.append("#include <stddef.h>")
    out.append("")

    # ------------------------------------------------------------------
    # Calendar samples
    # ------------------------------------------------------------------
    cal_rows: list[str] = []
    d = date(CAL_FIRST_YEAR, 1, 1)
    step = timedelta(days=23)
    boundary_days: set[date] = set()
    for year in range(CAL_FIRST_YEAR, CAL_LAST_YEAR + 1):
        table = jie_table(year)
        for term in JIE_TERMS:
            b = table[term]
            boundary_days.add(b)
    while d <= date(CAL_LAST_YEAR, 12, 31):
        for hour in (10, 23):
            facts = calendar_facts(d, hour)
            cal_rows.append(cal_row(d, hour, facts))
        d += step
    for b in sorted(boundary_days):
        for delta in (-1, 0, 1):
            bd = b + timedelta(days=delta)
            facts = calendar_facts(bd, 12)
            cal_rows.append(cal_row(bd, 12, facts))
    out.append("typedef struct {")
    out.append("    int year, month, day, hour;")
    out.append("    int day_index, month_branch, month_stem, year_stem, year_branch;")
    out.append("} liuyao_cal_sample_t;")
    out.append("")
    out.append(f"#define LIUYAO_CAL_SAMPLE_COUNT {len(cal_rows)}")
    out.append("static const liuyao_cal_sample_t LIUYAO_CAL_SAMPLES[] = {")
    out.extend(cal_rows)
    out.append("};")
    out.append("")

    # ------------------------------------------------------------------
    # Engine golden vectors
    # ------------------------------------------------------------------
    cases: list[tuple[list[int], int, int, int, int]] = []

    def add(lines: list[int], day: int, month_branch: int,
            category: int, perspective: int) -> None:
        cases.append((lines, day, month_branch, category, perspective))

    add([7, 7, 7, 7, 7, 7], 0, 0, 0, 0)
    add([8, 8, 8, 8, 8, 8], 59, 11, 1, 0)
    add([9, 9, 9, 9, 9, 9], 30, 5, 3, 1)
    add([6, 6, 6, 6, 6, 6], 30, 5, 3, 2)
    add([9, 7, 7, 7, 7, 7], 12, 2, 2, 0)     # 乾之姤
    add([7, 7, 7, 7, 7, 9], 12, 2, 2, 0)
    add([6, 8, 8, 8, 8, 8], 41, 8, 4, 0)
    add([8, 8, 8, 6, 8, 9], 5, 1, 6, 0)
    add([7, 9, 8, 8, 6, 8], 23, 3, 7, 0)
    add([9, 6, 7, 8, 8, 8], 17, 9, 8, 0)
    for _ in range(320):
        lines = [rng.randint(6, 9) for _ in range(6)]
        add(lines, rng.randint(0, 59), rng.randint(0, 11),
            rng.randint(0, len(CATEGORIES) - 1), rng.randint(0, 2))

    out.append("typedef struct {")
    out.append("    const char *record; /* input#expected */")
    out.append("} liuyao_engine_sample_t;")
    out.append("")
    out.append(f"#define LIUYAO_ENGINE_SAMPLE_COUNT {len(cases)}")
    out.append("static const char *const LIUYAO_ENGINE_SAMPLES[] = {")
    for lines, day, month_branch, category, perspective in cases:
        result = liuyao_core.build_chart(
            lines,
            day_ganzhi=STEMS[day % 10] + BRANCHES[day % 12],
            month_branch=BRANCHES[month_branch],
            cast_at="fixture",
            question_category=CATEGORIES[category],
            question_perspective=PERSPECTIVES[perspective],
        )
        inp = "L{};d{};m{};c{};p{}".format(
            "".join(str(v) for v in lines), day, month_branch,
            category, perspective)
        fp = fingerprint(result["chart"], result["analysis"])
        out.append(f'    "{inp}#{fp}",')
    out.append("};")
    out.append("")

    (REPO / "tests" / "liuyao_fixtures.h").write_text(
        "\n".join(out) + "\n", encoding="utf-8")
    print(f"wrote tests/liuyao_fixtures.h: "
          f"{len(cal_rows)} calendar samples, {len(cases)} engine cases")


def cal_row(d: date, hour: int, facts: dict[str, int]) -> str:
    return ("    {{ {:d}, {:d}, {:d}, {:d}, {:d}, {:d}, {:d}, {:d}, {:d} }},".format(
        d.year, d.month, d.day, hour,
        facts["day"], facts["month_branch"], facts["month_stem"],
        facts["year_stem"], facts["year_branch"]))


if __name__ == "__main__":
    main()
