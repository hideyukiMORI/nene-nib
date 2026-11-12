#pragma once

#include "ExLineRequest.hpp"
#include "PointerSelection.hpp"
#include "VimCommandMotion.hpp"
#include "VimKey.hpp"

#include <variant>

namespace nenenib::application
{
// engine の 1 歩の出どころ（Issue #1023）。鍵なら vim_step・`:N` の行なら vim_line_jumped・本文の
// ダブルクリックとトリプルクリックなら vim_pointer_selected（#1250）・オペレータ待ちの `:`
// の確定なら vim_command_operated（#1549）で、 engine を呼ぶ 1 か所が std::visit
// で写す。閉じた和型なので、出どころが増えたらコンパイルが落ちる （CPP-002）。std::function
// は空の呼び出しの _Xbad_function_call を application の外へ出す（ARC-003）。
using VimStepSource =
    std::variant<core::VimKey, core::ExLineRequest, PointerSelection, core::VimCommandMotion>;
} // namespace nenenib::application
