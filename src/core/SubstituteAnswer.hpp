#pragma once

#include "SubstituteConfirmation.hpp"
#include "SubstituteReply.hpp"

namespace nenenib::core
{
// `:s///c` の問いへの 1 つの答え（ADR 0168 の決定 6・段 2a）。application が問いの状態と答えを
// engine の vim_substitute_answered へ渡す 1 歩の出どころ。値は単独で妥当なので公開 aggregate
// （CPP-003）。
struct SubstituteAnswer
{
    SubstituteConfirmation confirmation;
    SubstituteReply reply;
};
} // namespace nenenib::core
