#pragma once

#include "VimCount.hpp"
#include "VimMode.hpp"
#include "VimPendingOperator.hpp"
#include "VimRegister.hpp"
#include "VimWantedColumn.hpp"

#include <optional>
#include <utility>

namespace nenenib::core
{
// Vim の状態（ADR 0012 の決定 1）。本文・キャレット・履歴は持たない（所有は
// EditorState・ARC-004）。 wanted_column
// が空なら「いまの桁」を欲しい列として使う。どの値も単独で妥当な公開 aggregate。
struct VimState
{
    VimMode mode;
    std::optional<VimCount> count;
    std::optional<VimPendingOperator> pending;
    std::optional<VimWantedColumn> wanted_column;
    VimRegister unnamed_register;
};

// 鍵を 1 つ食べ終わったあとの NORMAL。回数・オペレータ・欲しい列は空で、無名レジスタだけ残る。
// Vim モードに入るときも、通常モードへ戻して保留を捨てるときも、この 1 つの形に寄せる。
[[nodiscard]] inline VimState vim_resting_state(VimRegister unnamed_register)
{
    return VimState{VimMode::normal, std::nullopt, std::nullopt, std::nullopt,
                    std::move(unnamed_register)};
}
} // namespace nenenib::core
