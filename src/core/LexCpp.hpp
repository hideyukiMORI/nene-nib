#pragma once

#include "LexLine.hpp"
#include "LexState.hpp"

#include <string_view>

namespace nenenib::core
{
// C と C++ の 1 行の字句器（#815・ADR 0156 の決定 5・6）。入口は lex_line の 1 本で、ここを直接
// 呼ばない。分類は Vim の c.vim / cpp.vim に寄せ、ASCII の識別子だけを分類する。
[[nodiscard]] LexLine lex_cpp_line(const LexState &in, std::string_view line);
} // namespace nenenib::core
