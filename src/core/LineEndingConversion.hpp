#pragma once

#include "LineEnding.hpp"

#include <string>
#include <string_view>

namespace nenenib::core
{
// 保存の形が本文の改行の形と違うときに書くバイト列（ADR 0010 の追補 #1049 の決定 3・ADR 0036 の
// 決定 6 の追補）。crlf へは直前に `\r` の無い `\n` の前に `\r` を足し（`\r\n` はそのまま）、lf
// へは
// `\r\n` を `\n` にする。`\n` の前に無い単独の `\r`（LF 文書の literal CR）はどちらでも残す。
// LF 文書の literal CR の直後に `\n` が来る並びは、crlf にすると 1 つの CRLF になる。
[[nodiscard]] std::string line_endings_converted(std::string_view text, LineEnding to);
} // namespace nenenib::core
