#pragma once

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 読んだ量 1 つ（ADR 0184 の決定 2・VimPattern.cpp の parse の中だけで使う）。minimum と maximum は
// 回数（maximum が無ければ上限なし）、lazy は `\{-...}`（0 回の枝を先に試す）。`*` は {0, なし,
// 偽}、`\+` は {1, なし, 偽}、`\=` `\?` は {0, 1, 偽}。parse はこれを原子の写し
// （VimPatternRepeat）に開く。値は単独で妥当なので公開 aggregate（CPP-003）。
struct VimPatternQuantity
{
    std::size_t minimum;
    std::optional<std::size_t> maximum;
    bool lazy;
};
} // namespace nenenib::core
