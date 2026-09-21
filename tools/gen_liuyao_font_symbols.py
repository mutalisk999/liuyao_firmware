#!/usr/bin/env python3
"""Extract the Chinese/Latin character inventory of the liuyao firmware UI.

Scans every C string literal in the liuyao application sources (main/*.c that
carry UI or engine text) and writes the unique character set to
assets/fonts/liuyao-symbols.txt. That inventory is the font-subset contract:
regenerate fonts (tools/gen_liuyao_fonts.sh) whenever it changes.

    python tools/gen_liuyao_font_symbols.py
"""

from __future__ import annotations

import re
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SOURCES = [
    "main/main.c",
    "main/liuyao_app.c",
    "main/liuyao_engine.c",
    "main/liuyao_calendar.c",
    "main/liuyao_data.c",
    "main/liuyao_reading.c",
    "main/liuyao_theme.c",
    "main/liuyao_pages_flow.c",
    "main/liuyao_pages_cast.c",
    "main/liuyao_pages_result.c",
]

# 48px 封面大字只用于这几个字(独立小字库);24/16px 用全量清单。
COVER_SUBSET = "六爻"

STRING_RE = re.compile(r'"((?:[^"\\\n]|\\.)*)"')


def collect() -> str:
    chars: set[str] = set()
    for rel in SOURCES:
        path = REPO / rel
        if not path.exists():
            raise SystemExit(f"missing source: {rel}")
        text = path.read_text(encoding="utf-8")
        for literal in STRING_RE.findall(text):
            chars.update(literal)
    # 基础保障:可打印 ASCII、常用全角标点与特殊符号。
    chars.update(chr(c) for c in range(0x20, 0x7F))
    chars.update("，。、；：？！·—…《》「」()％‰℃")
    chars.update("○×○○✕")  # 动爻与装饰符号
    chars.discard("\n")
    chars.discard("\t")
    chars.discard("\\")
    return "".join(sorted(chars))


def main() -> None:
    all_chars = collect()
    out = REPO / "assets/fonts"
    out.mkdir(parents=True, exist_ok=True)
    (out / "liuyao-symbols.txt").write_text(all_chars, encoding="utf-8")
    (out / "liuyao-symbols-cover.txt").write_text(
        "".join(sorted(set(COVER_SUBSET))), encoding="utf-8")
    cjk = sum(1 for c in all_chars if ord(c) > 0x2E7F)
    print(f"inventory: {len(all_chars)} chars ({cjk} CJK) -> assets/fonts/")


if __name__ == "__main__":
    main()
