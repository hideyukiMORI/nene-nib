#pragma once

#include "Language.hpp"

#include <optional>

namespace nenenib::core
{
// `:set filetype=<名前>` `:set ft=<名前>` と `:set filetype?` `:set ft?`（ADR 0154 の決定 4）。
// language は選んだ言語で、空は「今の値を知らせる」問い。今の値は文書を持つ controller が答える。
struct ExLanguageRequest
{
    std::optional<Language> language;
};
} // namespace nenenib::core
