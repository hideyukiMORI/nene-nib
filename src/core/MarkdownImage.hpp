#pragma once

#include "Offset.hpp"

#include <cstddef>
#include <string>

namespace nenenib::core
{
// Markdown の画像の節（施主決定 D79・ADR 0155 の追補 47・#1381）。parse_markdown が本文の順に
// MarkdownDocument.images へ並べる（画像の中の画像も、外側が先）。block は画像を含む要素の番号、
// [begin, end) はその要素の text の中の「[画像: alt]」の全体（PreviewStyle::image の span と同じ範
// 囲）。source は参照先、title は題（無ければ空）で、どちらも実体参照を解いた md4c の属性の文字列。
// alt は「[画像: 」と「]」の間の文字（要素の text の写し）。どの欄も単独で妥当な値なので公開
// aggregate（CPP-003）。比較は非メンバー。
struct MarkdownImage
{
    std::size_t block;
    Offset begin;
    Offset end;
    std::string source;
    std::string title;
    std::string alt;
};

[[nodiscard]] inline bool operator==(const MarkdownImage &left, const MarkdownImage &right)
{
    return left.block == right.block && left.begin == right.begin && left.end == right.end &&
           left.source == right.source && left.title == right.title && left.alt == right.alt;
}
} // namespace nenenib::core
