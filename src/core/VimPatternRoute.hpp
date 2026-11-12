#pragma once

#include <cstdint>

namespace nenenib::core
{
// 照合器が通る道（ADR 0184 の決定 1・2）。sequence は量の無い列を後戻りせずに順に比べる道
// （matched_sequence）、states は量（`*` と開いた量の写し）を含む列の状態集合の道（今の
// expanded）、lazy_states は最短の量の写しを含む列の状態集合の道（lazily_expanded）。量の無い
// パターンは今の 2 本（sequence と states）のどちらかを通り、lazy_states を通らない（決定 1）。
enum class VimPatternRoute : std::uint8_t
{
    sequence,
    states,
    lazy_states
};
} // namespace nenenib::core
