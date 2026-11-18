#pragma once

#include <cstdint>

namespace nenenib::core
{
// プレビューの 1 要素の種類（ADR 0151 の決定 7）。leaf ブロック（段落・見出し・コードブロック・
// HTML ブロック・水平線と、箇条書きの項目の直下の文字）ごとに 1 つ。値が増えたら描画の switch
// が落ちる（CPP-002）。front_matter は文書の先頭の `---` … `---` / `...` の間
// （YAML）で、Markdown として読まない（#1840）。
enum class PreviewBlockKind : std::uint8_t
{
    paragraph,
    heading,
    code,
    html,
    rule,
    list_item,
    front_matter
};
} // namespace nenenib::core
