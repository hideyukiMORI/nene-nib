#pragma once

#include "HtmlFailure.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 変換する fragment の上限（ADR 0157 の決定 5）。超えたら too_large。
inline constexpr std::size_t html_fragment_limit = std::size_t{16} * 1024U * 1024U;

// クリップボードの HTML の fragment（UTF-8）を Markdown（改行は LF）に写す
// （ADR 0157 の決定 5〜7）。外部の parser を使わない自前の最小の字句器で、OS にも時刻にも
// 触れない純関数（ARC-007）。
// 対応する要素と出す記法は決定 6、閉じない要素は fragment の終わりで閉じたとみなす（決定 7）。
// 改行を文書の形に揃えるのは貼り付けの道（clipboard_line_feeds と同じ道）で、ここは LF だけ出す。
[[nodiscard]] std::expected<std::string, HtmlFailure> markdown_from_html(std::string_view fragment);
} // namespace nenenib::core
