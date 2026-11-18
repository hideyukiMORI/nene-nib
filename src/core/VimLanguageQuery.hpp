#pragma once

#include "LanguageQueryKind.hpp"

namespace nenenib::core
{
// `gd` `K` が言語サーバーへの問いを頼む（ADR 0190 の決定 8）。engine は port に触れず、この値を
// 返すだけで、問いを出すのは controller（ADR 0051 の形）。状態と失敗の印は表に行が無かった
// ときと同じまま（言語サーバーが動いていない文書では、今までどおりの失敗に見える）。
struct VimLanguageQuery
{
    LanguageQueryKind kind;
};
} // namespace nenenib::core
