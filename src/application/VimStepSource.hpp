#pragma once

#include "ExLineRequest.hpp"
#include "VimKey.hpp"

#include <variant>

namespace nenenib::application
{
// engine の 1 歩の出どころ（Issue #1023）。鍵なら vim_step・`:N` の行なら vim_line_jumped で、
// engine を呼ぶ 1 か所が std::visit で写す。閉じた和型なので、出どころが増えたらコンパイルが落ちる
// （CPP-002）。std::function は空の呼び出しの _Xbad_function_call を application
// の外へ出す（ARC-003）。
using VimStepSource = std::variant<core::VimKey, core::ExLineRequest>;
} // namespace nenenib::application
