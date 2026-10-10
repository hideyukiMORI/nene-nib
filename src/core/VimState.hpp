#pragma once

#include "VimBlockExtent.hpp"
#include "VimCharacterSearch.hpp"
#include "VimCount.hpp"
#include "VimInputWait.hpp"
#include "VimInsertRepeat.hpp"
#include "VimMacroRecording.hpp"
#include "VimMode.hpp"
#include "VimNamedRegisters.hpp"
#include "VimNumberedRegisters.hpp"
#include "VimPendingOperator.hpp"
#include "VimRegisterSelection.hpp"
#include "VimRegisterSnapshot.hpp"
#include "VimRepeatRecord.hpp"
#include "VimSearchHighlight.hpp"
#include "VimSearchSnapshot.hpp"
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
    // `"{name}` で選んだレジスタ（ADR 0048 の決定 2）。回数と同じく命令が完了するまで持ち、
    // vim_resting_from で消える（`register` は予約語なので selected_register）。
    std::optional<VimRegisterSelection> selected_register;
    std::optional<VimCharacterSearch> last_character_search;
    // 直前の検索（文字列・向き・解析結果の不変値、ADR 0102）。`n` / `N` がこれを使い、
    // 見つからなかった検索も覚える（次の `n` が同じ失敗を繰り返すのが Vim と同じ・実測）。
    std::optional<VimSearchSnapshot> last_search;
    // 検索の当たりを強調するか（ADR 0037 の決定 1）。既定は on で、Ex の `:set (no)hlsearch` と
    // `:nohlsearch` だけが変え、検索の鍵が suspended を on へ戻す。永続化はしない。
    VimSearchHighlight highlight;
    // 検索の入力中に当たりを preview として見せるか（ADR 0041 の決定 6）。既定は true で、
    // Ex の `:set (no)incsearch` だけが変える。engine はこれを読まず、hlsearch
    // と同じく永続化しない。
    bool incsearch;
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
    VimRegisterSnapshot unnamed_register;
    // 名前つきレジスタとマクロ（ADR 0046 の決定 1・ADR 0048 の決定 1・6）。registers は a〜z の
    // 本文の表（マクロも本文で持つ）、macro_recording は `q{a-z}` から `q` までの録画中の鍵、
    // last_macro は `@@` が繰り返す直前の名前。`.` の記録とは独立で、どれも鍵を食べ終わっても
    // （vim_resting_from でも）保つ。
    VimNamedRegisters registers;
    // 数字レジスタ `"0`〜`"9` と小削除 `"-`（ADR 0050 の決定 1）。名前つきと同じく
    // vim_resting_from が持ち越し、書くのは registers_written だけ。
    VimNumberedRegisters numbered;
    VimRegisterSnapshot small_delete;
    // `"+` `"*` の写し（ADR 0051 の決定 2）。controller が vim_clipboard_loaded で OS の本文を
    // 置き、engine は普通のレジスタとして読む。selected_register と同じく命令が終わると消え
    // （vim_resting_from は持ち越さない）、OS が正のままである。
    VimRegisterSnapshot clipboard;
    std::optional<VimMacroRecording> macro_recording;
    std::optional<char> last_macro;
};

// 鍵を 1 つ食べ終わったあとの NORMAL。回数・オペレータ・欲しい列は空で、無名レジスタだけ残る。
// Vim モードに入るときも、通常モードへ戻して保留を捨てるときも、この 1 つの形に寄せる。
[[nodiscard]] inline VimState vim_resting_state(VimRegisterSnapshot unnamed_register)
{
    return VimState{VimMode::normal,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    VimSearchHighlight::on,
                    true,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::move(unnamed_register),
                    VimNamedRegisters{},
                    VimNumberedRegisters{},
                    VimRegisterSnapshot::from(VimRegister{"", VimRegisterKind::uninitialized}),
                    VimRegisterSnapshot::from(VimRegister{"", VimRegisterKind::uninitialized}),
                    std::nullopt,
                    std::nullopt};
}

// rawの入口は一度だけ防御コピーし、snapshotを受ける正典へ渡す。
[[nodiscard]] inline VimState vim_resting_state(const VimRegister &unnamed_register)
{
    return vim_resting_state(VimRegisterSnapshot::from(unnamed_register));
}

// 通常の鍵の完了は 'scroll' の明示値と直前の文字検索・検索パターンと強調・incsearch
// の有無とマクロ（ADR 0046）を捨てない。 Vim モードへ初めて入る 初期化だけが vim_resting_state
// を直接使い、空の値から始める。文字待ちは持ち越さない。
[[nodiscard]] inline VimState vim_resting_from(const VimState &state,
                                               VimRegisterSnapshot unnamed_register)
{
    VimState next = vim_resting_state(std::move(unnamed_register));
    next.scroll_lines = state.scroll_lines;
    next.last_character_search = state.last_character_search;
    next.last_search = state.last_search;
    next.highlight = state.highlight;
    next.incsearch = state.incsearch;
    next.registers = state.registers;
    next.numbered = state.numbered;
    next.small_delete = state.small_delete;
    next.macro_recording = state.macro_recording;
    next.last_macro = state.last_macro;
    return next;
}

[[nodiscard]] inline VimState vim_resting_from(const VimState &state,
                                               const VimRegister &unnamed_register)
{
    return vim_resting_from(state, VimRegisterSnapshot::from(unnamed_register));
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
