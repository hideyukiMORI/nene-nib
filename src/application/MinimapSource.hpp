#pragma once

#include "AutosaveId.hpp"
#include "Language.hpp"

namespace nenenib::application
{
// ミニマップの窓の行を読む文書（ADR 0171 の決定 3・4）。tab は文書の退避番号、language は言語、
// lexing は字句器へ通すか（字句器を持つ言語で `:syntax on`）。覚えはこの 3 つが同じときだけ続ける。
// どの欄も単独で妥当な値なので公開 aggregate。
struct MinimapSource
{
    AutosaveId tab;
    core::Language language;
    bool lexing;
};

[[nodiscard]] constexpr bool operator==(const MinimapSource &left,
                                        const MinimapSource &right) noexcept
{
    return left.tab == right.tab && left.language == right.language && left.lexing == right.lexing;
}
} // namespace nenenib::application
