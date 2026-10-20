#pragma once

#include "VimAction.hpp"
#include "VimOperator.hpp"

namespace nenenib::core
{
// 動作 → オペレータの表の 1 行（CPP-012・ADR 0015 の決定 10・11）。保留を立てる鍵と、保留の
// 後ろで行単位になる鍵（dd の 2 打鍵目・g~~ の ~・guu の u・gUU の U）を同じ形で引く。
struct VimOperatorBinding
{
    VimAction action;
    VimOperator operation;
};
} // namespace nenenib::core
