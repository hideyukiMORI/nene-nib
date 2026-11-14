#pragma once

#include "ExLineRequest.hpp"
#include "ExSubstituteRequest.hpp"
#include "InsertRestartRequest.hpp"
#include "PointerSelection.hpp"
#include "SubstituteAnswer.hpp"
#include "VimCommandMotion.hpp"
#include "VimKey.hpp"

#include <variant>

namespace nenenib::application
{
// engine の 1 歩の出どころ（Issue #1023）。鍵なら vim_step・`:N` の行なら vim_line_jumped・本文の
// ダブルクリックとトリプルクリックなら vim_pointer_selected（#1250）・INSERT の `Ctrl-o` の始め直し
// なら vim_restarted_insert（ADR 0182）・オペレータ待ちの `:` の確定なら vim_command_operated
// （#1549）・`:s` の族なら vim_substituted（ADR 0168）・`:s///c` の問いの答えなら
// vim_substitute_answered（ADR 0168 の決定 6）で、engine を呼ぶ 1 か所が std::visit
// で写す。閉じた和型なので、出どころが増えたら コンパイルが落ちる（CPP-002）。std::function
// は空の呼び出しの _Xbad_function_call を application の外へ出す（ARC-003）。
using VimStepSource =
    std::variant<core::VimKey, core::ExLineRequest, PointerSelection, InsertRestartRequest,
                 core::VimCommandMotion, core::ExSubstituteRequest, core::SubstituteAnswer>;
} // namespace nenenib::application
