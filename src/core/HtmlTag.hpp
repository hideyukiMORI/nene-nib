#pragma once

#include <cstdint>

namespace nenenib::core
{
// markdown_from_html が見分ける要素の閉じた一覧（ADR 0157 の決定 6）。名前 → 値の表は
// HtmlToMarkdown.cpp の 1 つで、表に無い名前は other（中身の文字だけ）。heading_1〜heading_6 は
// この順に並べる（段の数は値の差で引く）。
enum class HtmlTag : std::uint8_t
{
    heading_1,
    heading_2,
    heading_3,
    heading_4,
    heading_5,
    heading_6,
    paragraph,
    line_break,
    strong,
    emphasis,
    anchor,
    unordered_list,
    ordered_list,
    list_item,
    quote,
    code,
    preformatted,
    rule,
    image,
    table,
    table_row,
    table_cell,
    // 記法を出さない塊（div と節の要素）。前後で改行するだけ。
    division,
    // 中身を持たない知らない要素（meta・input など）。何も出さない。
    void_other,
    // 知らない要素と span。中身の文字だけ。
    other
};
} // namespace nenenib::core
