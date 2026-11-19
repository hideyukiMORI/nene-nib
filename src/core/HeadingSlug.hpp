#pragma once

#include "MarkdownHeading.hpp"
#include "PreviewBlock.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 見出しの名札（slug）を GitHub の形で作る唯一の純関数（ADR 0151 の追補 2026-11-19 の段 L3・
// #1869）。blocks の見出しの要素（PreviewBlockKind::heading）ごとに 1 つ、文書の順に返す。
// 名札は表示の文字（PreviewBlock.text・装飾の記号を除いた後）から作る:
// - 前後の空白（ASCII の空白）を除く。
// - ASCII の大文字を小文字に畳む（非 ASCII の大小は畳まない）。
// - ASCII の英数字と `-` `_` と非 ASCII の字（日本語など）を残し、空白を `-` に（続く空白は
//   まとめない）、ほかの ASCII の字（記号・Tab・改行）を消す。
// - 非 ASCII のうち CJK と全角の句読点・記号（U+2010〜U+2027・U+3000〜U+303F・全角の記号）と
//   絵文字（U+1F000〜U+1FAFF・U+2600〜U+27BF・U+FE0F・U+200D）を消す（範囲は HeadingSlug.cpp の
//   removed_code_points の表 1 つ）。非 ASCII の大文字は畳まない（全角の英数字も残す）。
// - 同じ名札が 2 つ目から現れたら `-1` `-2` … を付ける（GitHub の公開の説明の「hyphen と
//   増える整数」。付けた形が既にあればさらに次の数へ進む・github-slugger と同じ数え方）。
// parse_markdown が文書を解析するときに 1 回だけ呼び、MarkdownDocument.headings に置く。
[[nodiscard]] std::vector<MarkdownHeading> heading_slugs(std::span<const PreviewBlock> blocks);

// リンクの宛先の `#` の後ろ（LinkDestination.text）で引く見出しの要素の番号。`%XX` を
// percent_decoded で解いてから、ASCII の大小を畳んで名札と比べる。同じ名札は無いので最初の 1 つ。
// 見つからなければ nullopt。
[[nodiscard]] std::optional<std::size_t> heading_block_of(std::span<const MarkdownHeading> headings,
                                                          std::string_view fragment);
} // namespace nenenib::core
