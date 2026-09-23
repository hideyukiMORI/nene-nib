#pragma once

#include "VimSearchDirection.hpp"

namespace nenenib::application
{
// incsearch の入力中の Ctrl-G / Ctrl-T（ADR 0043 の決定 2）。relative は本文の順で `/` `?` に
// 依らず、forward は下の当たり（Ctrl-G）、backward は上の当たり（Ctrl-T）。入力行に文字は入れない。
struct SearchHop
{
    core::VimSearchDirection relative;
};
} // namespace nenenib::application
