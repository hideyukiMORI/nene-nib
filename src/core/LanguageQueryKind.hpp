#pragma once

#include <cstdint>

namespace nenenib::core
{
// 言語サーバーへキャレットの位置で問う種類（ADR 0190 の決定 8）。Vim の `gd` `K` と操作
// 「定義へ移動」「情報を表示」が同じ値を運び、問いを出すのは application の 1 本。値が増えたら
// 写し先の switch が落ちる（CPP-002）。
enum class LanguageQueryKind : std::uint8_t
{
    definition,
    hover
};
} // namespace nenenib::core
