#include "VimStepDetail.hpp"

#include "VimMotionRange.hpp"
#include "VimTextObject.hpp"
#include "VimTextObjectCancel.hpp"
#include "VimTextObjectOutcome.hpp"
#include "VimTextObjectSpan.hpp"

#include <optional>
#include <utility>
#include <variant>

namespace nenenib::core
{
using detail::completed_text_object;
using detail::finished_input_wait;
using detail::performed;
using detail::rested_in;
using detail::visual_resting;
using detail::word_writing_empty;
using detail::writing_empty;

namespace
{
// ---------------------------------------------------------------- テキストオブジェクト（ADR 0031）

// オペレータの後ろ。exclusive 補正は通さず、d だけが Vi 互換の行単位の規則を通る（決定 3）。
// y のキャレットは範囲の先頭で、行単位の範囲でもその桁へ戻る（Vim 9.1 で実測）。
[[nodiscard]] VimStep operated_on_object(const VimState &state, const VimEditorView &view,
                                         const VimMotionRange &range)
{
    VimState from = state;
    from.input_wait = std::nullopt;
    from.wanted_column = std::nullopt;
    return performed(from, view.text, range.range.begin, range);
}

// VISUAL。両端とモードは範囲関数が決めた値をそのまま置く（決定 4 と Issue #99 の補足）。行単位に
// なる範囲でも語・引用符・括弧の VISUAL は文字単位のままで、caret が最後の行の内容の終わりに
// 載ると選択が改行まで届く（実測）。段落は行単位 VISUAL へ移る（Issue #729）。
[[nodiscard]] VimStep selected_object(const VimState &state, const VimEditorView &view,
                                      const VimTextObjectSpan &span)
{
    const Selection &selected = span.selection;
    VimState next = visual_resting(state, span.mode);
    next.wanted_column =
        VimWantedColumn{VimColumnWish::at_column, caret_virtual_column(view.text, selected.caret)};
    return VimStep{std::move(next), VimSelect{selected}};
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep completed_text_object(const VimState &state, const VimEditorView &view,
                                            const VimTextObjectSpan &span)
{
    if (state.pending.has_value())
    {
        return operated_on_object(state, view, span.range);
    }
    return selected_object(state, view, span);
}

// 範囲にならなかったとき（ADR 0031 の補足）。保留中のオペレータは捨て、キャレット（VISUAL では
// 選択）だけが走査の止まった所へ動く。本文は変わらないので `.` の記録はそのまま捨てられる
// （ADR 0030 の決定 3）。動いていなければ何も起きない鍵として返す。
[[nodiscard]] VimStep completed_text_object(const VimState &state, const VimEditorView &view,
                                            const VimTextObjectCancel &cancel)
{
    VimState next = finished_input_wait(state);
    if (!state.pending.has_value())
    {
        if (cancel.selection == view.selection)
        {
            return VimStep{std::move(next), VimNoEffect{}};
        }
        next.wanted_column = VimWantedColumn{
            VimColumnWish::at_column, caret_virtual_column(view.text, cancel.selection.caret)};
        return VimStep{std::move(next), VimSelect{cancel.selection}};
    }
    const Offset caret = rested_in(view.text, cancel.selection.caret, VimMode::normal);
    if (caret == view.selection.caret)
    {
        return VimStep{std::move(next), VimNoEffect{}};
    }
    next.wanted_column =
        VimWantedColumn{VimColumnWish::at_column, caret_virtual_column(view.text, caret)};
    return VimStep{std::move(next), VimMoveTo{caret}};
}

// 語の範囲（`iw` `iW` `aw` `aW`）。空行で空でも `c` は空の本文を書く（#979・VimEmptyChange）。
// Vim の current_word は空行の NUL を inclusive に選ぶので oap->empty にならず、op_change は空を
// yank する（実測）。括弧などのほかの空の範囲（`ci(` の `()`）は書かないので印を立てない。
[[nodiscard]] VimTextObjectOutcome word_writing_empty(VimTextObjectOutcome outcome,
                                                      VimTextObject object)
{
    if ((object == VimTextObject::word || object == VimTextObject::big_word) &&
        std::holds_alternative<VimTextObjectSpan>(outcome))
    {
        auto &span = std::get<VimTextObjectSpan>(outcome);
        span.range = writing_empty(span.range);
    }
    return outcome;
}
} // namespace detail
} // namespace nenenib::core
