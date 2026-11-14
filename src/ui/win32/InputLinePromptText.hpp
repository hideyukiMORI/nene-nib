#pragma once

#include "DisplayLanguage.hpp"
#include "InputLinePrompt.hpp"

#include <string>
#include <string_view>

namespace nenenib::ui::win32
{
// 入力行のプロンプトの文字（ADR 0032 の決定 1）。renderer が入力行の前に描く 1 本で、`:s///c` の
// 問い（ADR 0168 の決定 6）は文がそのまま Vim の文言なのでプロンプトを出さない。通常モードの 3 つは
// 表示言語の表から引く（ADR 0123 の追補 2026-11-13）。detail は「置換後 (語): 」の語。
[[nodiscard]] std::string prompt_text(core::InputLinePrompt prompt, std::string_view detail,
                                      core::DisplayLanguage language);
} // namespace nenenib::ui::win32
