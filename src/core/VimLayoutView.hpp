#pragma once

#include "VimLineRows.hpp"

#include <cstddef>
#include <span>

namespace nenenib::core
{
// 未測定の行はVimStep.layout_requestで求める。無効時は従来の1行1画面行。
struct VimLayoutView
{
    bool enabled = false;
    std::span<const VimLineRows> lines{};
};
} // namespace nenenib::core
