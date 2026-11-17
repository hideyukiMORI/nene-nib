#pragma once

#include "EasyMotionKind.hpp"

namespace nenenib::core
{
// `\` の後の鍵 → イージーモーションの動作の表の 1 行（ADR 0163 の決定 1・CPP-012）。
struct VimEasyMotionBinding
{
    char32_t key;
    EasyMotionKind kind;
};
} // namespace nenenib::core
