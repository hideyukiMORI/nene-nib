// 変更の印（ADR 0181・追補 #1374）。効果ごとの既定の位置 change_position と、印を立てる 1 か所
// change_marked をこの翻訳単位に置く（ADR 0158 の決定 3 の群・#1845 で VimStep.cpp から移した）。
// 呼ぶのは vim_step の出口・オペレータ待ちの `:` の確定・補完の面の行のクリックの 1 歩。
#include "VimStepDetail.hpp"

#include "TextPosition.hpp"
#include "VimChange.hpp"
#include "VimChangeMark.hpp"
#include "VimEditorView.hpp"
#include "VimEffect.hpp"
#include "VimLinesLeft.hpp"

#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
// changed の印の既定の位置（ADR 0181 の追補 #1374）: 効果が本文を置き換える範囲の始まり。Vim の
// ふつうの編集は changed_bytes / changed_lines に置き換えの始まりの行と桁を渡す（del_bytes・
// ins_char_bytes・op_shift の (start, 0)・do_join の (行, 元の行の長さ)・do_put の (行, col)・
// 行単位は (行, 0)）。範囲の始まりは置き換えの前後で同じ座標。矩形の効果はここへ来る前に作る所が
// Vim の op_* の位置を書く（change_placed）ので、ここは矩形の左上の行の行頭を返すだけ。
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimRemoveRange &effect)
{
    return view.text.position_of(effect.range.begin);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimRemoveLines &effect)
{
    return view.text.position_of(effect.range.begin);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimReplaceRange &effect)
{
    return view.text.position_of(effect.range.begin);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimInsertAt &effect)
{
    return view.text.position_of(effect.at);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimInsertString &)
{
    return view.text.position_of(view.selection.caret);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimNewLine &)
{
    return view.text.position_of(view.selection.caret);
}
[[nodiscard]] TextPosition block_line_start(const VimEditorView &view, Offset caret)
{
    return TextPosition{view.text.position_of(caret).line, Column{1}};
}
// 矩形の削除（op_delete）は check_cursor_col で詰めた後のカーソルそのもの（cursor.lnum,
// cursor.col）
// を渡すので、カーソルの行では桁の比べがいつも成り立つ。左上の行の行頭と同じ答えになる。
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimRemoveBlock &effect)
{
    return block_line_start(view, effect.caret);
}
// 矩形の大小（op_tilde）・字下げ（op_shift）・VISUAL の `<C-a>`（op_addsub）は (start.lnum, 0)。
// 矩形の `r`（op_replace の (start.lnum, start.col)）と INSERT の 1 行目の埋め（edit() の
// changed_bytes）は作る所が change_placed で書く。
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimReplaceBlock &effect)
{
    return block_line_start(view, effect.caret);
}
// 矩形の貼付（do_put の MBLOCK）は changed_lines(lnum, 0, ...) で lnum は貼った左上の行。矩形の
// INSERT の Esc（op_insert の block_insert・op_change）は 1 行目を edit() が済ませた後の
// changed_lines(start.lnum + 1, 0, ...)。
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimInsertBlock &effect)
{
    const TextPosition top = block_line_start(view, effect.caret);
    switch (effect.boundary)
    {
    // 矩形の効果は coalesce を作らない（通常モードの打鍵だけの区切り）。来たら貼付と同じに数える。
    case EditBoundary::coalesce:
    case EditBoundary::separate:
        return top;
    case EditBoundary::absorb:
        return TextPosition{LineNumber{top.line.value + 1}, Column{1}};
    }
    std::unreachable();
}
// 本文に触れない効果の changed（空の範囲の印）は鍵の前のカーソルの行と桁（Vim の op_shift の
// 空の行・op_change の空の行はカーソルの行の (行, 0) で、カーソルの桁はそれ以上）。効果の種類が
// 増えたら多重定義が足りずにコンパイルが落ちる（CPP-002）。
[[nodiscard]] TextPosition caret_position(const VimEditorView &view)
{
    return view.text.position_of(view.selection.caret);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimNoEffect &)
{
    return caret_position(view);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimMoveTo &)
{
    return caret_position(view);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimNavigate &)
{
    return caret_position(view);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimSelect &)
{
    return caret_position(view);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimUndo &)
{
    return caret_position(view);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimRedo &)
{
    return caret_position(view);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimOpenCommandLine &)
{
    return caret_position(view);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimOpenSearch &)
{
    return caret_position(view);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimReplay &)
{
    return caret_position(view);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimSwitchTab &)
{
    return caret_position(view);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimWindowCommand &)
{
    return caret_position(view);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const PaneSizing &)
{
    return caret_position(view);
}
[[nodiscard]] TextPosition change_position(const VimEditorView &view, const VimLanguageQuery &)
{
    return caret_position(view);
}
// `:s///c` の答えは置いた当たりの行頭を印に書いて返す（VimStepSubstituteConfirm.cpp）。
[[nodiscard]] TextPosition change_position(const VimEditorView &view,
                                           const VimConfirmSubstitute &effect)
{
    return effect.edits.empty() ? caret_position(view)
                                : view.text.position_of(effect.edits.front().range.begin);
}

// 行単位の削除の演算子が 1 行目から最後の行までを消した印（ADR 0188 の決定 3 (a)・removed_exactly
// が行で立てる）を仕上げる。行が 1 つも無い本文（ML_EMPTY）の `dd` は何も消さないので外し、残った
// 印の鍵は本文を変える（del_lines の changed_lines）ので changed の印も立てる。
[[nodiscard]] VimStep lines_emptied(const VimEditorView &view, VimStep step)
{
    switch (step.lines_left)
    {
    case VimLinesLeft::some:
        return step;
    case VimLinesLeft::none:
        break;
    }
    if (vim_has_no_lines(view))
    {
        step.lines_left = VimLinesLeft::some;
        return step;
    }
    if (!step.change.has_value() || step.change.value().kind != VimChange::changed)
    {
        step.change = VimChangeMark{VimChange::changed, std::nullopt};
    }
    return step;
}
} // namespace

namespace detail
{
// 本文を置き換える編集の効果を返す step の印（ADR 0181 の決定 2）。編集を返す鍵は Vim が必ず
// u_save と changed_* を呼ぶので、印を書く所を効果ごとに散らさず出口のこの 1 か所で立てる。
// 編集を返さない形の印（saved_only・空の範囲の changed）は効果を作る所が立てたまま残す。changed の
// 印に位置が無ければ、ここで既定の位置（change_position）を書く（追補 #1374）。
VimStep change_marked(const VimEditorView &view, VimStep step)
{
    step = lines_emptied(view, std::move(step));
    if (vim_replaces_text(step.effect) &&
        (!step.change.has_value() || step.change.value().kind != VimChange::changed))
    {
        step.change = VimChangeMark{VimChange::changed, std::nullopt};
    }
    if (step.change.has_value() && step.change.value().kind == VimChange::changed &&
        !step.change.value().at.has_value())
    {
        step.change.value().at = std::visit([&view](const auto &effect)
                                            { return change_position(view, effect); }, step.effect);
    }
    return step;
}
} // namespace detail
} // namespace nenenib::core
