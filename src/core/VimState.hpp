#pragma once

#include "VimBlockExtent.hpp"
#include "VimCharacterSearch.hpp"
#include "VimCount.hpp"
#include "VimInputWait.hpp"
#include "VimInsertRepeat.hpp"
#include "VimMode.hpp"
#include "VimPendingOperator.hpp"
#include "VimRegister.hpp"
#include "VimRepeatRecord.hpp"
#include "VimSearchPattern.hpp"
#include "VimWantedColumn.hpp"

#include <cstddef>
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
    std::optional<VimInputWait> input_wait;
    std::optional<VimCharacterSearch> last_character_search;
    // 直前の検索（パターンと打たれた向き・ADR 0032 の決定 3）。`n` / `N` がこれを使い、
    // 見つからなかった検索も覚える（次の `n` が同じ失敗を繰り返すのが Vim と同じ・実測）。
    std::optional<VimSearchPattern> last_search;
    std::optional<VimWantedColumn> wanted_column;
    // Ctrl-d / Ctrl-u に明示した window-local な移動量。現在の viewport ではなく、次の
    // half-page command に残る Vim の 'scroll' に相当する値（ADR 0019 の決定 6）。
    std::optional<VimCount> scroll_lines;
    std::optional<VimInsertRepeat> insert_repeat;
    // `.` の記録（ADR 0030 の決定 1）。recording はいま組み立て中の命令の鍵、last_change は
    // 確定した直前の変更。どちらも vim_step の 1 か所だけが書き換える。
    std::optional<VimRepeatRecord> recording;
    std::optional<VimRepeatRecord> last_change;
    // `.` が矩形の変更を再生しているあいだだけ値を持つ（ADR 0035 の決定 7）。固定 Vim の
    // redo_VIsual_busy と同じで、再生の矩形は左上と幅を「選択の角」ではなく記録から取る
    // （短い行へ畳まれた角からは幅が読めない・Issue #112 で実測）。
    std::optional<VimBlockExtent> replayed_block;
    VimRegister unnamed_register;
};

// 鍵を 1 つ食べ終わったあとの NORMAL。回数・オペレータ・欲しい列は空で、無名レジスタだけ残る。
// Vim モードに入るときも、通常モードへ戻して保留を捨てるときも、この 1 つの形に寄せる。
[[nodiscard]] inline VimState vim_resting_state(VimRegister unnamed_register)
{
    return VimState{VimMode::normal,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::move(unnamed_register)};
}

// 通常の鍵の完了は 'scroll' の明示値と直前の文字検索・検索パターンを捨てない。Vim
// モードへ初めて入る 初期化だけが vim_resting_state
// を直接使い、空の値から始める。文字待ちは持ち越さない。
[[nodiscard]] inline VimState vim_resting_from(const VimState &state, VimRegister unnamed_register)
{
    VimState next = vim_resting_state(std::move(unnamed_register));
    next.scroll_lines = state.scroll_lines;
    next.last_character_search = state.last_character_search;
    next.last_search = state.last_search;
    return next;
}

// Vim の window-local 'scroll' は実際の表示高が変わったときだけ半画面の既定へ戻る。
// 同じ高さの通知は状態を変えない（ADR 0019 の決定 6）。
[[nodiscard]] inline VimState vim_after_resize(const VimState &state, std::size_t before,
                                               std::size_t after)
{
    if (before == after)
    {
        return state;
    }
    VimState next = state;
    next.scroll_lines = std::nullopt;
    return next;
}
} // namespace nenenib::core
