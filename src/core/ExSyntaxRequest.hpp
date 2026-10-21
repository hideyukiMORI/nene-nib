#pragma once

#include <optional>

namespace nenenib::core
{
// `:syntax on` `:syntax off` と `:syntax?`・引数の無い `:syntax`（ADR 0156 の追補・#829）。
// enabled は選んだ値で、空は「今の値を知らせる」問い。今の値は文書を持つ controller が答える。
struct ExSyntaxRequest
{
    std::optional<bool> enabled;
};
} // namespace nenenib::core
