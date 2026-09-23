#pragma once

#include "VimSearchDirection.hpp"

namespace nenenib::application
{
// incsearch の入力中の Ctrl-G / Ctrl-T（ADR 0043 の決定 2）。relative は検索の向きに相対で、
// forward は検索の向きの次の当たり（Ctrl-G）、backward は前の当たり（Ctrl-T）。`?` の入力中の
// forward は本文の上へ動く。入力行に文字は入れない。
struct SearchHop
{
    core::VimSearchDirection relative;
};
} // namespace nenenib::application
