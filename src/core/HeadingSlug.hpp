#pragma once

#include "HeadingSource.hpp"
#include "MarkdownHeading.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 見出しの名札（slug）を GitHub の形で作る唯一の純関数（ADR 0151 の追補 2026-11-19 の段 L3・
// #1869）。sources の見出しごとに 1 つ、同じ順に返す。名札は github-slugger 2.0.0 の slug() と同じ
// 3 つだけで作る:
// - ASCII の大文字を小文字に畳む（github-slugger は toLowerCase で非 ASCII も畳むが、GitHub
// の実物は
//   ASCII だけを畳む・ADR の限界）。
// - github-slugger の regex.js が消す code point を消す（表は生成物 HeadingSlugTable.hpp の 1 つ・
//   python eng/heading-slug-table.py --write）。
// - 空白（U+0020）を `-` に（続く空白はまとめない）。
// 前後の空白は除かない（md4c が見出しの前後の空白を渡さない。除いたタグの後ろの空白は GitHub と
// 同じく `-` になる）。
// 同じ名札が 2 つ目から現れたら `-1` `-2` … を付ける（GitHub の公開の説明の「hyphen と
// 増える整数」。付けた形が既にあればさらに次の数へ進む・github-slugger と同じ数え方）。
// parse_markdown が文書を解析するときに 1 回だけ呼び、MarkdownDocument.headings に置く。
[[nodiscard]] std::vector<MarkdownHeading> heading_slugs(std::span<const HeadingSource> sources);

// リンクの宛先の `#` の後ろ（LinkDestination.text）で引く見出しの要素の番号。`%XX` を
// percent_decoded で解いてから、ASCII の大小を畳んで名札と比べる。同じ名札は無いので最初の 1 つ。
// 見つからなければ nullopt。
[[nodiscard]] std::optional<std::size_t> heading_block_of(std::span<const MarkdownHeading> headings,
                                                          std::string_view fragment);
} // namespace nenenib::core
