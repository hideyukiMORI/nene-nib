#pragma once

#include "DisplayText.hpp"

#include <optional>
#include <string_view>

namespace nenenib::core
{
// 外から来た文（言語サーバーの診断の文と `window/showMessage`）を知らせの 1 行にする唯一の 1 本
// （ADR 0161 の決定 4・5）。最初の改行（LF か CR）の前までを取り、ほかの制御文字（Tab など）は
// 空白に置き換え、DisplayText::maximum_bytes（256 バイト）を超えれば code point の境目で切る。
// 空・UTF-8 として正しくない文は値なし（知らせを出さない）。
[[nodiscard]] std::optional<DisplayText> notice_line(std::string_view text);
} // namespace nenenib::core
