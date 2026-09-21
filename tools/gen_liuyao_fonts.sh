#!/usr/bin/env bash
# Generate the liuyao application LVGL font subsets from the character
# inventory produced by tools/gen_liuyao_font_symbols.py.
#
# Font source: Noto Sans SC (SIL OFL 1.1, redistributable), subset at
# assets/fonts/NotoSansSC-Regular.otf.
# Converter: official lv_font_conv (see docs/development/engineering/
# lvgl-chinese-fonts.md). Output is uncompressed 4bpp LVGL C sources that
# main/CMakeLists.txt compiles into the application.
#
# Usage:  ./tools/gen_liuyao_fonts.sh   (requires node/npx)
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
fonts_dir="${repo_root}/assets/fonts"
font_file="${fonts_dir}/NotoSansSC-Regular.otf"

if [[ ! -f "${font_file}" ]]; then
    echo "ERROR: ${font_file} missing. See assets/README.md for the download." >&2
    exit 1
fi
for inv in liuyao-symbols.txt liuyao-symbols-cover.txt; do
    if [[ ! -f "${fonts_dir}/${inv}" ]]; then
        echo "ERROR: run tools/gen_liuyao_font_symbols.py first (${inv})." >&2
        exit 1
    fi
done

gen() {
    local size="$1" symbols="$2" name="$3"
    npx --yes lv_font_conv \
        --font "${font_file}" \
        --size "${size}" --bpp 4 --format lvgl --no-compress \
        --lv-font-name "${name}" --lv-include lvgl.h \
        --symbols "$(cat "${fonts_dir}/${symbols}")" \
        --output "${fonts_dir}/${name}.c"
    echo "wrote ${fonts_dir}/${name}.c (size ${size})"
}

# 16px:全部 UI 字符;24px:标题/卦名(全量清单,保证任意文案可作标题);
# 48px:封面大字独立小清单。
gen 16 liuyao-symbols.txt liuyao_font_16
gen 24 liuyao-symbols.txt liuyao_font_24
gen 48 liuyao-symbols-cover.txt liuyao_font_48
