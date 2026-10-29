#pragma once

#include "MarkdownTask.hpp"
#include "Offset.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewSpan.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::core
{
// プレビューの 1 要素（ADR 0151 の決定 7・8）。enter / leave の積みを平らにした値で、深さは数で
// 持つ（255 で飽和）。level は見出しの段（1〜6・ほかは 0）。marker は項目の最初の要素だけが持つ
// 記号（番号つきは "1."・箇条書きは "•"・ほかは空）。source_begin は要素の最初の文字の本文の
// バイト位置。text は実体参照を解いた表示の文字で、ハードブレークは "\n"、
// 段落の中の改行は空白 1 つ。task はタスクリストの項目の最初の要素だけが持つチェックボックス
// （ADR 0155 の追補 22・ほかは nullopt）。
struct PreviewBlock
{
    PreviewBlockKind kind;
    std::uint8_t level;
    std::uint8_t quote_depth;
    std::uint8_t list_depth;
    std::string marker;
    Offset source_begin;
    std::string text;
    std::vector<PreviewSpan> spans;
    std::optional<MarkdownTask> task;
};

[[nodiscard]] inline bool operator==(const PreviewBlock &left, const PreviewBlock &right)
{
    return left.kind == right.kind && left.level == right.level &&
           left.quote_depth == right.quote_depth && left.list_depth == right.list_depth &&
           left.marker == right.marker && left.source_begin == right.source_begin &&
           left.text == right.text && left.spans == right.spans && left.task == right.task;
}
} // namespace nenenib::core
