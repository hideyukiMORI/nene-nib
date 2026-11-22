#pragma once

#include "Language.hpp"

#include <cstddef>

namespace nenenib::application
{
// 言語ごとの失敗の控え（ADR 0160 の決定 6 の 2026-11-20 の追補・段 S2）。failures は続けて失敗した
// 回数（`initialized` まで届くか `:set lsp` で 0 に戻る）、given_up は上限を超えて止まったこと。
// ある言語の命令の誤りでほかの言語のサーバーを止めない。公開 aggregate（CPP-003）。
struct LanguageServerTrouble
{
    core::Language language;
    std::size_t failures = 0;
    bool given_up = false;
};
} // namespace nenenib::application
