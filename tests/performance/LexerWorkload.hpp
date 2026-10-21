#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// 行の字句器 lex_line（#815 の C++・#830 の JSON と Python・#833 の Markdown と JavaScript・
// 段 2 の ADR 0156）。言語ごとに 1 行 100 バイトの 6 種の行を繰り返す。visible_60 は 1 枠ぶんの
// 60 行を区間ごと読み、backfill_200 は見えている行の前の 200 行を lex_state で状態だけ次へ渡して
// 読む（区間を作らない・段 2 の遡りの形）。
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
    javascript_backfill_200
};
} // namespace nenenib::tests::performance
