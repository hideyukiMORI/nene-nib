#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// 行の字句器 lex_line（#815 の C++・#830 の JSON と Python・#833 の Markdown と JavaScript・
// 段 2 の ADR 0156）。言語ごとに 1 行 100 バイトの 6 種の行を繰り返す。visible_60 は 1 枠ぶんの
// 60 行を区間ごと読み、backfill_200 は見えている行の前の 200 行を lex_state で状態だけ次へ渡して
// 読む（区間を作らない・段 2 の遡りの形）。markdown_worst_line は閉じの無い `[` を 16,384 文字
// 並べた 1 行（#836 のレビュー R1・1 行の走査が O(n) のまま）。#838 の HTML / XML・CSS・YAML・
// TOML は visible_60 を 5 つ、backfill_200 を HTML と YAML と TOML の 3 つ。
enum class LexerWorkload : std::uint8_t
{
    cpp_visible_60,
    cpp_backfill_200,
    json_visible_60,
    json_backfill_200,
    python_visible_60,
    python_backfill_200,
    markdown_visible_60,
    markdown_backfill_200,
    javascript_visible_60,
    javascript_backfill_200,
    markdown_worst_line,
    html_visible_60,
    html_backfill_200,
    xml_visible_60,
    css_visible_60,
    yaml_visible_60,
    yaml_backfill_200,
    toml_visible_60,
    toml_backfill_200
};
} // namespace nenenib::tests::performance
