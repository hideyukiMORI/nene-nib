#pragma once

#include "VimCount.hpp"

#include <optional>

namespace nenenib::core
{
// `"{name}` の前の回数（Vim 9.1 の normal.c の nv_regname が覚える cap->opcount・#1319）。
// before は `"` の前までの回数の積で、typed は `"{name}` の後に打ち直した桁。Vim は `"` の後で
// count0 を 0 から数え直し（normal_cmd_get_count の `0` は count0 が 0 でない間だけ桁）、次の
// 命令の前に count0 に opcount を掛ける。VimState::count は readers が見る積（before × typed、
// typed が無ければ before）のままで、桁の読みだけがこれを見る。
struct VimRegisterCount
{
    VimCount before;
    std::optional<VimCount> typed;
};
} // namespace nenenib::core
