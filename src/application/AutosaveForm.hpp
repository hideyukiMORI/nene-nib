#pragma once

#include "LineEnding.hpp"
#include "TextEncoding.hpp"

#include <optional>

namespace nenenib::application
{
// 退避した時点の文書の形（ADR 0137 の追補 #1688）。退避の本文は形によらず UTF-8（本文のまま）で、
// 戻すときは検出せずにこの 3 つを置く。encoding は文書の文字コード（Document.encoding）、save は
// 保存の改行の形（Document.line_ending・値なしは本文の形のまま）、body は本文の改行の形
// （TextBuffer の line_ending）。値だけを運ぶ公開 aggregate（CPP-003）。
struct AutosaveForm
{
    core::TextEncoding encoding;
    std::optional<core::LineEnding> save;
    core::LineEnding body;
};
} // namespace nenenib::application
