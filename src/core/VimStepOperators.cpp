#include "VimStepDetail.hpp"

#include "Utf8.hpp"
#include "VimAction.hpp"
#include "VimCaret.hpp"
#include "VimCase.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimCharacterSearchInvocation.hpp"
#include "VimKeyTable.hpp"
#include "VimMotionRange.hpp"
#include "VimNumberedRule.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
using detail::adjusted_for_exclusive;
using detail::backward_characters;
using detail::cancelled;
using detail::caret_on_line;
using detail::case_changed;
using detail::cased_text;
using detail::changed;
using detail::character_search_position;
using detail::characters_between;
using detail::completed_character_search;
using detail::count_of;
using detail::failed;
using detail::finished_input_wait;
using detail::forward_characters;
using detail::inclusive_end;
using detail::joined;
using detail::jumped_by;
using detail::keeps_the_line_break;
using detail::landing_of;
using detail::line_of;
using detail::lines_below;
using detail::motion_range;
using detail::motion_step;
using detail::moved_by;
using detail::operated;
using detail::operated_on_lines;
using detail::operated_to_line_end;
using detail::operation_of;
using detail::opposite;
using detail::pending_operator;
using detail::performed;
using detail::refused_register;
using detail::registers_written;
using detail::removed_exactly;
using detail::repeated_character_search;
using detail::resolved_count;
using detail::resolved_motion_count;
using detail::restored_at;
using detail::searches_forward;
using detail::ShiftBy;
using detail::shifted;
using detail::shifted_line;
using detail::single_step;
using detail::started_character_search;
using detail::started_prefix;
using detail::swapped_characters;
using detail::text_object_action;
using detail::to_line_end;
using detail::visual_restore;
using detail::wanted_column_of;
using detail::whole_lines_for_delete;
using detail::with_clipboard;
using detail::WrittenRegisters;
using detail::yanked;

namespace
{
constexpr char carriage_return = '\r';

constexpr char line_feed_byte = '\n';

// キャレットの下に空白でない文字があるか（Vim の gchar_cursor() != NUL && !VIM_ISWHITE）。
// 行の内容の終わり（NUL の桁）と空行は「文字が無い」ので偽。cw の特例が効く条件そのもの。
[[nodiscard]] bool word_under_caret(const TextBuffer &text, Offset caret)
{
    const LineNumber line = line_of(text, caret);
    if (caret.value >= text.line_end(line).value)
    {
        return false;
    }
    const std::string under = text.text_range(caret, forward_characters(text, caret, single_step));
    return under != " " && under != "\t";
}

// ---------------------------------------------------------------- オペレータ（決定 2・3）

// レジスタの本文は LF だけ（決定 3）。落とすのは CRLF の文書の改行の '\r' だけで、LF の文書の
// literal CR は文字なので残る（ADR 0036 の決定 2。行の切り方と同じ規則）。
[[nodiscard]] std::string without_newline_carriage_returns(std::string_view text, LineEnding ending)
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const bool of_newline = ending == LineEnding::crlf && text.at(index) == carriage_return &&
                                index + 1 < text.size() && text.at(index + 1) == line_feed_byte;
        if (!of_newline)
        {
            result.push_back(text.at(index));
        }
    }
    return result;
}

[[nodiscard]] VimRegister register_of(const TextBuffer &text, const VimMotionRange &range)
{
    std::string body = without_newline_carriage_returns(
        text.text_range(range.range.begin, range.range.end), text.line_ending());
    switch (range.kind)
    {
    case VimRegisterKind::uninitialized:
    case VimRegisterKind::block:
        // Motion ranges are constructed only as characterwise or linewise.
        std::unreachable();
    case VimRegisterKind::lines:
        // 行単位のレジスタは必ず改行で終わる（Vim は最終行にも付ける）。
        body.push_back('\n');
        return VimRegister{std::move(body), VimRegisterKind::lines};
    case VimRegisterKind::characters:
        return VimRegister{std::move(body), VimRegisterKind::characters};
    }
    std::unreachable();
}

// 空の文字単位の範囲はレジスタを書き換えない（空行の D が実測でそう振る舞う）。ただし行末への
// 移動と語のテキストオブジェクトの範囲を `c` が消すときは空の本文を書く（Vim の op_delete は
// OP_DELETE だけ空行で先に返り、op_change は空を yank する・#960・#979・ADR 0050 の追補）。
[[nodiscard]] std::optional<VimRegister>
register_after(const TextBuffer &text, const VimMotionRange &range, VimOperator operation)
{
    if (range.kind == VimRegisterKind::characters && is_empty(range.range))
    {
        if (operation == VimOperator::change && range.empty_change == VimEmptyChange::writes_empty)
        {
            return VimRegister{std::string{}, VimRegisterKind::characters};
        }
        return std::nullopt;
    }
    return register_of(text, range);
}

// yank 系の `"A` の追記（ADR 0048 の決定 4）。どちらかが行単位なら行単位で、文字単位の後ろへ
// 行を繋ぐときは間に改行を、行単位の後ろへ文字を繋ぐときは末尾に改行を足す。未使用なら新しい
// 値そのもの。矩形が絡む追記は書かない（Vim と違う・後続）。
[[nodiscard]] std::optional<VimRegister> appended_register(const VimRegister &old,
                                                           const VimRegister &value)
{
    switch (old.kind)
    {
    case VimRegisterKind::uninitialized:
        return value;
    case VimRegisterKind::block:
        return std::nullopt;
    case VimRegisterKind::characters:
    case VimRegisterKind::lines:
        break;
    }
    switch (value.kind)
    {
    case VimRegisterKind::uninitialized:
        return old;
    case VimRegisterKind::block:
        return std::nullopt;
    case VimRegisterKind::characters:
    case VimRegisterKind::lines:
        break;
    }
    if (old.kind == value.kind)
    {
        return VimRegister{old.text + value.text, old.kind};
    }
    std::string text = old.kind == VimRegisterKind::characters ? old.text + "\n" + value.text
                                                               : old.text + value.text + "\n";
    return VimRegister{std::move(text), VimRegisterKind::lines};
}

// 名指しの書き込み（ADR 0050 の決定 3 の 1）。名前つきは表へ（追記なら ADR 0048 の決定 4 で
// 繋ぐ・繋げなければ refused）、数字はその位置へ、`"-` は小削除へ置き換える。`""` と選択の無い
// yank は `"0` へ置く。選択の無い削除・変更はここでは何も書かない。`"_` は呼び出し元が先に返す。
// `"+` `"*` は表へ置かない（OS へ出す本文は registers_written が組にして返す・ADR 0051 の決定 6）。
[[nodiscard]] std::expected<VimState, VimRepeatFailure>
named_written(VimState state, const VimRegisterSnapshot &value, VimOperator operation)
{
    if (!state.selected_register.has_value())
    {
        switch (operation)
        {
        case VimOperator::yank:
            state.numbered.registers.at(0) = value;
            return state;
        case VimOperator::remove:
        case VimOperator::change:
        // 大小文字と字下げはレジスタを書かない（ADR 0015 の決定 10・11）。ここへは来ない。
        case VimOperator::toggle_case:
        case VimOperator::lower_case:
        case VimOperator::upper_case:
        case VimOperator::shift_right:
        case VimOperator::shift_left:
            return state;
        }
        std::unreachable();
    }
    const VimRegisterSelection selection = state.selected_register.value();
    const auto name = static_cast<char32_t>(selection.name);
    switch (selection.target)
    {
    case VimRegisterTarget::black_hole:
        return state;
    case VimRegisterTarget::unnamed:
        state.numbered.registers.at(0) = value;
        return state;
    case VimRegisterTarget::numbered:
        state.numbered.registers.at(vim_numbered_index(name).value_or(0)) = value;
        return state;
    case VimRegisterTarget::small_delete:
        state.small_delete = value;
        return state;
    case VimRegisterTarget::clipboard:
        return state;
    case VimRegisterTarget::named:
        break;
    }
    VimRegisterSnapshot &target =
        state.registers.registers.at(vim_register_index(name).value_or(0));
    if (!selection.append)
    {
        target = value;
        return state;
    }
    const std::optional<VimRegister> written = appended_register(target.value(), value.value());
    if (!written.has_value())
    {
        return std::unexpected(VimRepeatFailure::refused);
    }
    target = VimRegisterSnapshot::from(written.value());
    return state;
}

// レジスタの値が 1 行に収まらないか（行単位か、本文に改行を含む・ADR 0050 の決定 4）。複数行の
// 矩形の本文は行の区切りの改行を含む。「`"1` へ行くか」はこの 1 か所が値から決める。
[[nodiscard]] bool spans_lines(const VimRegister &value) noexcept
{
    return value.kind == VimRegisterKind::lines || value.text.contains('\n');
}

// `"1` の規則（決定 3 の 2）。削除と変更で、値が 1 行に収まらないか検索の移動なら、`"1`〜`"8` を
// `"2`〜`"9` へ繰り下げて（`"9` は捨てる）`"1` へ今回の値を置く。
[[nodiscard]] VimState shifted_into_first(VimState state, const VimRegisterSnapshot &value,
                                          VimOperator operation, VimNumberedRule rule)
{
    if (operation == VimOperator::yank ||
        (rule == VimNumberedRule::by_extent && !spans_lines(value.value())))
    {
        return state;
    }
    auto &numbered = state.numbered.registers;
    std::shift_right(std::next(numbered.begin()), numbered.end(), 1);
    numbered.at(1) = value;
    return state;
}

// `"-` の規則（決定 3 の 3）。選択の無い削除と変更で、値が 1 行に収まるときだけ小削除へ置く。
[[nodiscard]] VimState small_delete_written(VimState state, const VimRegisterSnapshot &value,
                                            VimOperator operation)
{
    if (operation == VimOperator::yank || state.selected_register.has_value() ||
        spans_lines(value.value()))
    {
        return state;
    }
    state.small_delete = value;
    return state;
}

// 無名へ写す値（決定 3 の 4）。追記なら表の追記後の全体、ほかは今回の値。
[[nodiscard]] VimRegisterSnapshot unnamed_written(const VimState &state,
                                                  const VimRegisterSnapshot &value)
{
    if (!state.selected_register.has_value())
    {
        return value;
    }
    const VimRegisterSelection selection = state.selected_register.value();
    if (selection.target != VimRegisterTarget::named || !selection.append)
    {
        return value;
    }
    return state.registers.registers.at(
        vim_register_index(static_cast<char32_t>(selection.name)).value_or(0));
}

// OS へ出す本文（決定 6・追補（#848）の決定 C）。名指しの書き込みの書き先が OS のクリップボード
// のときと、`:set clipboard=unnamed` でレジスタを選ばずに無名へ書くとき（`"a` の名指しは Vim と
// 同じく出さない）。どちらでも 1 回だけ。
[[nodiscard]] std::optional<VimRegister> clipboard_written(const VimState &state,
                                                           const VimRegister &value)
{
    if (!state.selected_register.has_value())
    {
        return state.clipboard_unnamed ? std::optional<VimRegister>{value} : std::nullopt;
    }
    if (state.selected_register.value().target != VimRegisterTarget::clipboard)
    {
        return std::nullopt;
    }
    return value;
}
} // namespace

namespace detail
{
// 削除・変更・yank の書き先（ADR 0050 の決定 3・ADR 0048 の決定 3 を改める）。書き手はすべて
// ここを通る（ARC-001）。`"_` と値が無い（空の文字単位の範囲）ときはどのレジスタも変えず、
// ほかは「名指しの書き込み → `"1` の規則 → `"-` の規則 → 無名」の順に埋める。`"+` `"*` の
// 名指しの書き込みは状態へ置かず、OS へ出す本文として組の second に返す（ADR 0051 の決定 6）。
// `:set clipboard=unnamed` でレジスタを選ばない書きも、無名と同じ本文を second に返す（追補の決定
// C）。 命令が完了した状態を返すので、選んだレジスタと写しはここで消える。
[[nodiscard]] std::expected<WrittenRegisters, VimRepeatFailure>
registers_written(const VimState &state, const std::optional<VimRegister> &value,
                  VimOperator operation, VimNumberedRule rule)
{
    const bool black_hole = state.selected_register.has_value() &&
                            state.selected_register.value().target == VimRegisterTarget::black_hole;
    if (!value.has_value() || black_hole)
    {
        return WrittenRegisters{vim_resting_from(state, state.unnamed_register), std::nullopt};
    }
    const VimRegisterSnapshot snapshot = VimRegisterSnapshot::from(value.value());
    auto named = named_written(state, snapshot, operation);
    if (!named.has_value())
    {
        return std::unexpected(named.error());
    }
    const VimState written = small_delete_written(
        shifted_into_first(std::move(named).value(), snapshot, operation, rule), snapshot,
        operation);
    return WrittenRegisters{vim_resting_from(written, unnamed_written(written, snapshot)),
                            clipboard_written(state, value.value())};
}

// OS へ出す本文を 1 打鍵の結果に添える（ADR 0051 の決定 6）。状態と効果は変えない。
[[nodiscard]] VimStep with_clipboard(VimStep step, std::optional<VimRegister> clipboard)
{
    step.clipboard = std::move(clipboard);
    return step;
}

// 書けなかった命令（矩形が絡む追記・決定 4）と名前にならない鍵（決定 2）。回数と保留と選んだ
// レジスタを捨ててビープする。VISUAL は選択を保ったまま残る（ほかの効かない鍵と同じ）。
[[nodiscard]] VimStep refused_register(const VimState &state)
{
    VimState next = vim_resting_from(state, state.unnamed_register);
    switch (state.mode)
    {
    case VimMode::normal:
    case VimMode::insert:
        break;
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        next.mode = state.mode;
        break;
    }
    return failed(VimStep{std::move(next), VimNoEffect{}}, VimRepeatFailure::refused);
}
} // namespace detail

namespace
{
// 行を消す範囲。最終行を消すときだけ 1 つ前の行の末尾から始める（改行を 1 つだけ消すため）。
[[nodiscard]] OffsetRange removed_lines_range(const TextBuffer &text, const VimMotionRange &range)
{
    const LineNumber first = line_of(text, range.range.begin);
    const LineNumber last = line_of(text, range.range.end);
    const bool through_end = last.value >= text.line_count();
    const Offset begin = through_end && first.value > 1 ? text.line_end(LineNumber{first.value - 1})
                                                        : range.range.begin;
    return OffsetRange{begin, text.line_terminator_end(last)};
}
} // namespace

namespace detail
{
// u と Ctrl-r の戻り先を 1 打鍵の結果に添える（ADR 0052 の決定 7）。状態と効果は変えない。
[[nodiscard]] VimStep restored_at(VimStep step, Offset restore)
{
    step.restore = restore;
    return step;
}
} // namespace detail

namespace
{
// `c` の戻り先（ADR 0052 の決定 9）。行単位で 2 行以上なら、範囲の戻り先の 1 行下の同じバイトの
// 桁（Vim の op_change は残りの行を消す間だけカーソルを 1 行下げて最初の保存をする・実測）。
[[nodiscard]] Offset change_restore(const TextBuffer &text, const VimMotionRange &range)
{
    const Offset restore = range.restore.value_or(range.range.begin);
    const LineNumber first = line_of(text, range.range.begin);
    if (range.kind != VimRegisterKind::lines || line_of(text, range.range.end) == first)
    {
        return restore;
    }
    const LineNumber line = line_of(text, restore);
    return vim_line_and_column(text, LineNumber{line.value + single_step},
                               restore.value - text.line_start(line).value);
}
} // namespace

namespace detail
{
// VISUAL の u と Ctrl-r の戻り先（ADR 0052 の決定 10）。文字単位は選択の先頭。行単位は先頭行の
// 上で、固定の端が先頭行なら行頭（1 行だけの `Vd` も・実測）、動く端だけが先頭行ならその位置。
// 矩形は左上（block_operated と replaced_block が書く）。
[[nodiscard]] Offset visual_restore(const TextBuffer &text, const Selection &selection,
                                    VimMode mode)
{
    const Offset first =
        selection.anchor.value < selection.caret.value ? selection.anchor : selection.caret;
    if (mode != VimMode::visual_line)
    {
        return first;
    }
    const LineNumber line = line_of(text, first);
    return line_of(text, selection.anchor) == line ? text.line_start(line) : selection.caret;
}

// 範囲をそのまま消す。VISUAL の `d` `x` はここへ直接来る（Vim の op_delete の「奇妙な Vi の
// 振る舞い」は `!oap->is_VIsual` で守られていて、選択からの削除には掛からない。Issue #53 で実測）。
[[nodiscard]] VimStep removed_exactly(const VimState &state, const TextBuffer &text,
                                      const VimMotionRange &range)
{
    auto written = registers_written(state, register_after(text, range, VimOperator::remove),
                                     VimOperator::remove, range.numbered);
    if (!written.has_value())
    {
        return refused_register(state);
    }
    auto [next, clipboard] = std::move(written).value();
    switch (range.kind)
    {
    case VimRegisterKind::uninitialized:
    case VimRegisterKind::block:
        // Motion ranges are constructed only as characterwise or linewise.
        std::unreachable();
    case VimRegisterKind::characters:
        return restored_at(with_clipboard(VimStep{std::move(next), VimRemoveRange{range.range}},
                                          std::move(clipboard)),
                           range.restore.value_or(range.range.begin));
    case VimRegisterKind::lines:
        return restored_at(with_clipboard(VimStep{std::move(next),
                                                  VimRemoveLines{removed_lines_range(text, range)}},
                                          std::move(clipboard)),
                           range.restore.value_or(range.range.begin));
    }
    std::unreachable();
}
} // namespace detail

namespace
{
[[nodiscard]] VimStep removed(const VimState &state, const TextBuffer &text,
                              const VimMotionRange &range)
{
    return removed_exactly(state, text, whole_lines_for_delete(text, range));
}
} // namespace

namespace detail
{
// c。効果は削除そのままで、次の状態が INSERT（決定 2）。行単位でも改行は残す＝行が 1 本残る。
[[nodiscard]] VimStep changed(const VimState &state, const TextBuffer &text,
                              const VimMotionRange &range)
{
    auto written = registers_written(state, register_after(text, range, VimOperator::change),
                                     VimOperator::change, range.numbered);
    if (!written.has_value())
    {
        return refused_register(state);
    }
    auto [next, clipboard] = std::move(written).value();
    next.mode = VimMode::insert;
    return restored_at(
        with_clipboard(VimStep{std::move(next), VimRemoveRange{range.range}}, std::move(clipboard)),
        change_restore(text, range));
}
} // namespace detail

namespace
{
// y のあとのキャレット。行単位VISUALは下向き・単一行なら先頭、上向きなら現在位置。
// NORMALの行単位は同じ桁を保つ。VISUAL終了時の行末の寄せはcontrollerの共通経路を使う。
[[nodiscard]] Offset yanked_caret(const TextBuffer &text, const VimState &state, Offset caret,
                                  const VimMotionRange &range)
{
    switch (range.kind)
    {
    case VimRegisterKind::uninitialized:
    case VimRegisterKind::block:
        // Motion ranges are constructed only as characterwise or linewise.
        std::unreachable();
    case VimRegisterKind::characters:
        return range.range.begin;
    case VimRegisterKind::lines:
        if (state.mode == VimMode::visual_line)
        {
            return line_of(text, caret) == line_of(text, range.range.end) ? range.range.begin
                                                                          : caret;
        }
        return caret_on_line(text, wanted_column_of(text, state, caret),
                             line_of(text, range.range.begin), VimMode::normal);
    }
    std::unreachable();
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep yanked(const VimState &state, const TextBuffer &text, Offset caret,
                             const VimMotionRange &range)
{
    auto written =
        registers_written(state, register_of(text, range), VimOperator::yank, range.numbered);
    if (!written.has_value())
    {
        return refused_register(state);
    }
    auto [next, clipboard] = std::move(written).value();
    return with_clipboard(
        VimStep{std::move(next), VimMoveTo{yanked_caret(text, state, caret, range)}},
        std::move(clipboard));
}
} // namespace detail

namespace
{
// ------------------------------------------- 大小文字と字下げと行の結合（決定 9〜11）

// 範囲が覆う最後の行。文字単位の範囲が次の行の行頭で終わるとき（`v$` が改行まで選んだとき）は、
// その行に文字が無いので前の行まで（Vim の oap->end の行）。
[[nodiscard]] LineNumber last_covered_line(const TextBuffer &text, const VimMotionRange &range)
{
    const LineNumber first = line_of(text, range.range.begin);
    const LineNumber last = line_of(text, range.range.end);
    const bool at_next_line_start = range.kind == VimRegisterKind::characters &&
                                    last.value > first.value &&
                                    range.range.end == text.line_start(last);
    return at_next_line_start ? LineNumber{last.value - single_step} : last;
}

// 大小文字が書き換える本文の範囲（決定 10）。行単位は覆う行の内容の全体、文字単位は範囲そのもの。
// 文字単位の空の範囲（1 桁目の `h` `0`・最初の非空白での `^`）も Vim は実行する（cpo-E が既定で
// 無い・op_tilde は oap->empty を見ない・#1064）。exclusive の終わりを 1 文字戻した位置までの
// 個数なので、本文の先頭なら着地の 1 文字、2 行目以降の 1 桁目は前の行の末尾へ戻るので行の
// 残り全体、それ以外の桁は 0 文字になる（Vim 9.1 の実測）。
[[nodiscard]] OffsetRange case_span(const TextBuffer &text, const VimMotionRange &range)
{
    if (range.kind == VimRegisterKind::lines)
    {
        return OffsetRange{text.line_start(line_of(text, range.range.begin)),
                           text.line_end(last_covered_line(text, range))};
    }
    const Offset at = range.range.begin;
    if (!is_empty(range.range) || at != text.line_start(line_of(text, at)))
    {
        return range.range;
    }
    return OffsetRange{at, at.value == 0 ? forward_characters(text, at, single_step)
                                         : text.line_end(line_of(text, at))};
}

// 1 code point の大小の写し（決定 10・Issue #781）。写像は Unicode の簡単な写像の表の 1 つ
// （VimCase）で、写像の無い code point はそのまま。
[[nodiscard]] char32_t cased(char32_t value, VimOperator operation) noexcept
{
    switch (operation)
    {
    case VimOperator::toggle_case:
        return vim_toggled(value);
    case VimOperator::lower_case:
        return vim_to_lower(value);
    case VimOperator::upper_case:
        return vim_to_upper(value);
    case VimOperator::remove:
    case VimOperator::change:
    case VimOperator::yank:
    case VimOperator::shift_right:
    case VimOperator::shift_left:
        return value;
    }
    std::unreachable();
}
} // namespace

namespace detail
{
// Vim の 1 文字（ADR 0053）ごとに基底の code point だけを写し、続く結合文字はバイト列のまま残す
// （Vim の swapchar が見るのは基底の文字だけ・結合文字つきの é は É ＋結合文字になる）。
[[nodiscard]] std::string cased_text(std::string_view text, VimOperator operation)
{
    std::string result;
    result.reserve(text.size());
    Offset at{0};
    while (at.value < text.size())
    {
        const Offset end = vim_character_end(text, at);
        const Offset marks = next_code_point(text, at);
        append_utf8(result, cased(code_point_at(text, at), operation));
        result.append(text.substr(marks.value, end.value - marks.value));
        at = end;
    }
    return result;
}

// 大小文字の置き換え（決定 10）。範囲は case_span の 1 本。本文は LF 換算で、controller が文書の
// 改行へ直して 1 Edit にする。キャレットは範囲の戻り先（行単位はキャレットと着地の小さい方・
// ないときは範囲の先頭・Vim の oap->start）で、u の戻り先も同じ。書き換える本文が空でも変更と
// して返す（`.` の記録に残り、キャレットは範囲の先頭へ・空の置き換えは controller が履歴に積まない
// ・#1064）。
[[nodiscard]] VimStep case_changed(const VimState &state, const TextBuffer &text,
                                   const VimMotionRange &range, VimOperator operation)
{
    const OffsetRange span = case_span(text, range);
    const Offset caret = range.restore.value_or(range.range.begin);
    std::string body = cased_text(
        without_newline_carriage_returns(text.text_range(span.begin, span.end), text.line_ending()),
        operation);
    return restored_at(VimStep{vim_resting_from(state, state.unnamed_register),
                               VimReplaceRange{span, std::move(body), caret}},
                       caret);
}
} // namespace detail

namespace
{
// 行の字下げの桁（Tab と空白の表示の桁・Vim の get_indent）と、字下げのバイト数。
[[nodiscard]] std::pair<std::size_t, std::size_t> indent_of(std::string_view line) noexcept
{
    std::size_t columns = 0;
    std::size_t bytes = 0;
    while (bytes < line.size() && (line[bytes] == ' ' || line[bytes] == '\t'))
    {
        columns += column_advance(static_cast<char32_t>(line[bytes]), columns);
        ++bytes;
    }
    return {columns, bytes};
}
} // namespace

namespace detail
{
// 1 行の字下げを shiftwidth × amount だけ動かして作り直す（決定 11・Vim の shift_line と
// set_indent。noshiftround・noexpandtab）。空行は動かさない（Vim の op_shift）。左は 0 で止まる。
// 作り直す字下げは vim_tab_stop ごとの Tab と残りの空白。大きすぎる字下げは書かない。
[[nodiscard]] std::optional<std::string> shifted_line(std::string_view line, VimOperator operation,
                                                      std::size_t amount)
{
    if (line.empty())
    {
        return std::string{};
    }
    const auto [columns, bytes] = indent_of(line);
    const std::size_t largest = std::numeric_limits<std::size_t>::max();
    if (amount > (largest - columns) / vim_shift_width)
    {
        return std::nullopt;
    }
    const std::size_t width = vim_shift_width * amount;
    const bool left = operation == VimOperator::shift_left;
    const std::size_t indent = left ? (columns < width ? 0 : columns - width) : columns + width;
    std::string result;
    if (indent / vim_tab_stop + indent % vim_tab_stop > result.max_size() - line.size())
    {
        return std::nullopt;
    }
    result.append(indent / vim_tab_stop, '\t');
    result.append(indent % vim_tab_stop, ' ');
    result.append(line.substr(bytes));
    return result;
}
} // namespace detail

namespace
{
// 置き換えた本文の最初の行の最初の非空白（Vim の beginline(BL_SOL | BL_FIX)）。空白だけの行は
// 最後の文字・空行は行頭。
[[nodiscard]] std::size_t first_line_first_non_blank(std::string_view body)
{
    const std::size_t index = indent_of(body).second;
    const bool at_line_end = index == body.size() || body.at(index) == '\n';
    return at_line_end && index > 0 ? previous_code_point(body, Offset{index}).value : index;
}
} // namespace

namespace detail
{
// > < の置き換え（決定 11）。範囲が覆う行を 1 行ずつ作り直し、最初の行から最後の行の内容の終わり
// までを 1 つの置き換えにする（undo 1 単位）。キャレットは最初の行の最初の非空白、u の戻り先は
// 範囲の戻り先。
[[nodiscard]] VimStep shifted(const VimState &state, const TextBuffer &text,
                              const VimMotionRange &range, ShiftBy shift)
{
    const auto [operation, amount] = shift;
    const LineNumber first = line_of(text, range.range.begin);
    const LineNumber last = last_covered_line(text, range);
    std::string body;
    for (std::size_t line = first.value; line <= last.value; ++line)
    {
        const auto moved = shifted_line(text.line_text(LineNumber{line}), operation, amount);
        if (!moved.has_value())
        {
            return failed(cancelled(state), VimRepeatFailure::too_large);
        }
        if (line != first.value)
        {
            body.push_back('\n');
        }
        body += moved.value();
    }
    const OffsetRange span{text.line_start(first), text.line_end(last)};
    const Offset caret{span.begin.value + first_line_first_non_blank(body)};
    return restored_at(VimStep{vim_resting_from(state, state.unnamed_register),
                               VimReplaceRange{span, std::move(body), caret}},
                       range.restore.value_or(range.range.begin));
}
} // namespace detail

namespace
{
// 行の最後から数えたバイト（無ければ 0）。J の空白の規則が見るのは ASCII の空白・Tab・`.` `?` `!`
// だけで、多バイト文字のバイトは 0x80 以上なのでどれにも当たらない（Vim の endcurr1 / endcurr2）。
[[nodiscard]] char byte_from_end(std::string_view part, std::size_t back) noexcept
{
    return part.size() > back ? part.at(part.size() - back - single_step) : '\0';
}

// J がつなぐ所に入れる空白の数（決定 9・Vim の do_join）。次の行の内容（先頭の空白を落とした後）が
// 空か `)` で始まるか、ここまでの本文が空か、前の行が Tab で終わるときは入れない。前の行が空白で
// 終わるときは足さない。joinspaces（Vim 9.1 の既定はオン）で前の行が `.` `?` `!` で終わる（空白の
// 手前を見る）なら 1 つ足す。
[[nodiscard]] std::size_t join_spaces(std::string_view body, std::string_view previous,
                                      std::string_view next) noexcept
{
    char last = byte_from_end(previous, 0);
    if (next.empty() || next.front() == ')' || body.empty() || last == '\t')
    {
        return 0;
    }
    std::size_t spaces = 0;
    if (last == ' ')
    {
        last = byte_from_end(previous, single_step);
    }
    else
    {
        ++spaces;
    }
    return last == '.' || last == '?' || last == '!' ? spaces + single_step : spaces;
}
} // namespace

namespace detail
{
// J と gJ（決定 9）。キャレットの行から回数の行（最小 2）をつなぐ。回数が本文の末尾を越えるとき、
// 2 行なら失敗し、3 行以上なら最後の行まで（Vim の nv_join）。J は次の行の先頭の空白を落として
// join_spaces の空白を入れ、gJ は何も落とさず入れない。キャレットは最後につないだ所（入れた空白の
// 始まり・行の外なら最後の文字・Vim の do_join）。レジスタは書かない。u の戻り先は押したときの
// キャレット（controller の既定）。
[[nodiscard]] VimStep joined(const VimState &state, const VimEditorView &view, bool inserts_space)
{
    const TextBuffer &text = view.text;
    const LineNumber first = line_of(text, view.selection.caret);
    std::size_t count = std::max<std::size_t>(count_of(state.count), 2);
    const std::size_t remaining = text.line_count() - first.value + single_step;
    if (count > remaining)
    {
        if (count <= 2)
        {
            return failed(cancelled(state), VimRepeatFailure::not_moved);
        }
        count = remaining;
    }
    const LineNumber last{first.value + count - single_step};
    std::string previous = text.line_text(first);
    std::string body = previous;
    std::size_t joint = 0;
    for (std::size_t line = first.value + single_step; line <= last.value; ++line)
    {
        std::string part = text.line_text(LineNumber{line});
        std::size_t spaces = 0;
        if (inserts_space)
        {
            part.erase(0, indent_of(part).second);
            spaces = join_spaces(body, previous, part);
        }
        joint = body.size();
        body.append(spaces, ' ');
        body += part;
        previous = std::move(part);
    }
    if (joint == body.size() && joint > 0)
    {
        joint = previous_code_point(body, Offset{joint}).value;
    }
    const OffsetRange span{text.line_start(first), text.line_end(last)};
    return VimStep{vim_resting_from(state, state.unnamed_register),
                   VimReplaceRange{span, std::move(body), Offset{span.begin.value + joint}}};
}

// `~`（決定 10・tildeop off）。キャレットから回数の文字（結合文字ごと）を反転し、行末で止まる。
// キャレットは反転した次の文字（行の外なら最後の文字・Vim の n_swapchar）。空行は失敗。
[[nodiscard]] VimStep swapped_characters(const VimState &state, const TextBuffer &text,
                                         Offset caret)
{
    const LineNumber line = line_of(text, caret);
    if (text.line_start(line) == text.line_end(line))
    {
        return failed(cancelled(state), VimRepeatFailure::not_moved);
    }
    const Offset end = forward_characters(text, caret, count_of(state.count));
    std::string body = cased_text(text.text_range(caret, end), VimOperator::toggle_case);
    const Offset landing =
        end == text.line_end(line) ? backward_characters(text, end, single_step) : end;
    return VimStep{vim_resting_from(state, state.unnamed_register),
                   VimReplaceRange{OffsetRange{caret, end}, std::move(body), landing}};
}

// いま効かせるオペレータ。保留が無ければ「消す」＝ x が通る道（x は保留を立てずにここへ来る）。
[[nodiscard]] VimOperator operation_of(const VimState &state) noexcept
{
    return state.pending.has_value() ? state.pending.value().operation : VimOperator::remove;
}

[[nodiscard]] VimStep performed(const VimState &state, const TextBuffer &text, Offset caret,
                                const VimMotionRange &range)
{
    switch (operation_of(state))
    {
    case VimOperator::remove:
        return removed(state, text, range);
    case VimOperator::change:
        return changed(state, text, range);
    case VimOperator::yank:
        return yanked(state, text, caret, range);
    case VimOperator::toggle_case:
    case VimOperator::lower_case:
    case VimOperator::upper_case:
        return case_changed(state, text, range, operation_of(state));
    case VimOperator::shift_right:
    case VimOperator::shift_left:
        return shifted(state, text, range, ShiftBy{operation_of(state), single_step});
    }
    std::unreachable();
}

[[nodiscard]] VimState finished_input_wait(const VimState &state)
{
    VimState next = state;
    next.count = std::nullopt;
    next.pending = std::nullopt;
    next.input_wait = std::nullopt;
    next.selected_register = std::nullopt;
    return next;
}
} // namespace detail

namespace
{
[[nodiscard]] VimState recorded_character_search(const VimState &state, VimCharacterSearch search,
                                                 VimCharacterSearchInvocation invocation)
{
    VimState next = state;
    next.input_wait = std::nullopt;
    if (invocation == VimCharacterSearchInvocation::first)
    {
        next.last_character_search = search;
    }
    return next;
}

[[nodiscard]] VimMotionRange character_search_range(const TextBuffer &text, Offset caret,
                                                    Offset destination, VimCharacterSearchKind kind)
{
    if (searches_forward(kind))
    {
        return characters_between(caret, inclusive_end(text, destination));
    }
    return characters_between(destination, caret);
}

[[nodiscard]] VimStep moved_by_character_search(const VimState &state, const VimEditorView &view,
                                                Offset destination)
{
    VimState next = finished_input_wait(state);
    next.wanted_column =
        VimWantedColumn{VimColumnWish::at_column, caret_virtual_column(view.text, destination)};
    switch (state.mode)
    {
    case VimMode::normal:
    case VimMode::insert:
        return VimStep{std::move(next), VimMoveTo{destination}};
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return VimStep{std::move(next), VimSelect{Selection{view.selection.anchor, destination}}};
    }
    std::unreachable();
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep completed_character_search(const VimState &state, const VimEditorView &view,
                                                 VimCharacterSearch search,
                                                 VimCharacterSearchInvocation invocation)
{
    const VimState recorded = recorded_character_search(state, search, invocation);
    const auto destination = character_search_position(view, recorded, search, invocation);
    if (!destination.has_value())
    {
        return failed(VimStep{finished_input_wait(recorded), VimNoEffect{}},
                      VimRepeatFailure::not_found);
    }
    if (recorded.pending.has_value())
    {
        return performed(recorded, view.text, view.selection.caret,
                         character_search_range(view.text, view.selection.caret,
                                                destination.value(), search.kind));
    }
    return moved_by_character_search(recorded, view, destination.value());
}

[[nodiscard]] VimStep started_character_search(const VimState &state, VimCharacterSearchKind kind)
{
    VimState next = state;
    next.input_wait = VimInputWait{kind};
    return VimStep{std::move(next), VimNoEffect{}};
}

[[nodiscard]] VimStep started_prefix(const VimState &state, VimPrefix prefix)
{
    VimState next = state;
    next.input_wait = VimInputWait{prefix};
    return VimStep{std::move(next), VimNoEffect{}};
}
} // namespace detail

namespace
{
// オペレータ保留中と VISUAL の i / a（ADR 0031 の決定 1）。次の鍵まで待つだけで本文は動かない。
[[nodiscard]] VimStep started_text_object(const VimState &state, VimTextObjectScope scope)
{
    VimState next = state;
    next.input_wait = VimInputWait{scope};
    return VimStep{std::move(next), VimNoEffect{}};
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep text_object_action(const VimState &state, VimAction action)
{
    return started_text_object(state, action == VimAction::insert_before
                                          ? VimTextObjectScope::inner
                                          : VimTextObjectScope::around);
}

[[nodiscard]] VimStep repeated_character_search(const VimState &state, const VimEditorView &view,
                                                VimAction action)
{
    if (!state.last_character_search.has_value())
    {
        return failed(VimStep{finished_input_wait(state), VimNoEffect{}},
                      VimRepeatFailure::refused);
    }
    VimCharacterSearch search = state.last_character_search.value();
    if (action == VimAction::repeat_character_search_opposite)
    {
        search.kind = opposite(search.kind);
    }
    return completed_character_search(state, view, search, VimCharacterSearchInvocation::repeat);
}
} // namespace detail

namespace
{
// cw cW の特例（Vim の nv_wordcmd）。語の上（非空白）では ce cE の範囲になり、空白の上は
// dw dW と同じ。
[[nodiscard]] VimMotion motion_for_change(const TextBuffer &text, Offset caret,
                                          VimOperator operation, VimMotion motion)
{
    const bool word_motion = motion == VimMotion::next_word || motion == VimMotion::next_big_word;
    if (operation != VimOperator::change || !word_motion || !word_under_caret(text, caret))
    {
        return motion;
    }
    return motion == VimMotion::next_word ? VimMotion::word_end_for_change
                                          : VimMotion::big_word_end_for_change;
}

// 行単位の移動の範囲の戻り先（ADR 0052 の決定 8）。先頭行にあるのはキャレットと着地の小さい方
// で、その位置で Vim は保存する（`dk` は着地の桁・`dj` はキャレットの桁）。
[[nodiscard]] VimMotionRange landed(const VimEditorView &view, const VimState &state,
                                    VimMotion motion, VimMotionRange range)
{
    if (range.kind == VimRegisterKind::lines)
    {
        const Offset destination = moved_by(view, state, motion);
        const Offset caret = view.selection.caret;
        range.restore = destination.value < caret.value ? destination : caret;
    }
    return range;
}

// 範囲にならなかったオペレータ。移動が失敗つきで動いていたら（回数の途中で本文の先頭に当たった
// b B ge gE）Vim と同じくキャレットだけ着地へ動かし、オペレータは打ち消す（Issue #224）。
[[nodiscard]] VimStep cancelled_at_landing(const VimState &state, const VimEditorView &view,
                                           VimMotion motion)
{
    const VimMotionLanding landing = landing_of(view, state, motion);
    if (landing.failure.has_value() && landing.offset != view.selection.caret)
    {
        return motion_step(state, view, motion);
    }
    return failed(cancelled(state), VimRepeatFailure::not_moved);
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep operated(const VimState &state, const VimEditorView &view, VimMotion motion)
{
    const VimOperator operation = operation_of(state);
    const VimMotion wanted = motion_for_change(view.text, view.selection.caret, operation, motion);
    const auto range =
        motion_range(view, wanted, resolved_motion_count(state, view.text, wanted), operation);
    if (!range.has_value())
    {
        return cancelled_at_landing(state, view, wanted);
    }
    VimState performed_from = state;
    // 行単位の y は移動の前の位置と着地の (行, 桁) で早い方へ着く（Vim の op_yank は範囲の始まり
    // oap->start へキャレットを置き、do_pending_operator は LT_POS で行と桁を比べる）。上へ動いた
    // 移動と、同じ行の手前に着く移動（`$ygg` `j$yG` `$y'a`）は着地の桁: `-` gg G H M L 'a は最初の
    // 非空白、`k` は欲しい列を保つ（Issue #411 / #965 / #1065・ADR 0015 の追補）。下へ動いた y と
    // 同じ行の後ろに着く y は範囲の始まり＝キャレットのまま。本文の位置の順は (行, 桁) の順と同じ。
    // `_` は y の下ではカーソルを動かさない（Vim の nv_lineop は OP_YANK で beginline を呼ばない・
    // `j$y_` は `$` のまま）ので、同じ行の最初の非空白を着地として比べない（決定 13）。
    if (operation == VimOperator::yank && range.value().kind == VimRegisterKind::lines &&
        wanted != VimMotion::first_non_blank_below)
    {
        const Offset destination = moved_by(view, state, wanted);
        if (destination.value < view.selection.caret.value)
        {
            performed_from.wanted_column = VimWantedColumn{
                VimColumnWish::at_column, caret_virtual_column(view.text, destination)};
        }
    }
    // 飛ぶ移動はオペレータの後ろでも前の位置を置く（Vim の setpcmark・ADR 0152 の決定 4）。
    return jumped_by(
        performed(performed_from, view.text, view.selection.caret,
                  keeps_the_line_break(view.text, range.value(), wanted, operation)
                      ? range.value()
                      : adjusted_for_exclusive(view.text,
                                               landed(view, state, wanted, range.value()), wanted)),
        view, wanted);
}

// オペレータを自分の回数といっしょに保留する（決定 1）。積んだ回数はここで移る。
[[nodiscard]] VimStep pending_operator(const VimState &state, VimOperator operation)
{
    VimState next = state;
    next.count = std::nullopt;
    next.pending = VimPendingOperator{operation, state.count};
    next.input_wait = std::nullopt;
    return VimStep{std::move(next), VimNoEffect{}};
}

// dd cc yy と Y。自分の行から count-1 行下まで。保留を立ててから 1 本の performed に流す。
[[nodiscard]] VimStep operated_on_lines(const VimState &state, const TextBuffer &text, Offset caret,
                                        VimOperator operation)
{
    auto range = lines_below(text, line_of(text, caret), resolved_count(state) - single_step);
    if (!range.has_value())
    {
        return failed(cancelled(state), VimRepeatFailure::not_moved);
    }
    // 着地は範囲の最後の行の最初の非空白。1 行ならキャレットと着地の小さい方（ADR 0052 の決定 8）。
    const Offset landing = vim_first_non_blank(text, range.value().range.end);
    range.value().restore = landing.value < caret.value ? landing : caret;
    VimState with = state;
    with.pending = VimPendingOperator{operation, std::nullopt};
    return performed(with, text, caret, range.value());
}

// D と C。d$ / c$ と同じ経路（決定 6）。回数は $ が受け取る（2D が 2 行ぶんになる）。
[[nodiscard]] VimStep operated_to_line_end(const VimState &state, const TextBuffer &text,
                                           Offset caret, VimOperator operation)
{
    VimState with = state;
    with.pending = VimPendingOperator{operation, std::nullopt};
    const auto range = to_line_end(text, caret, resolved_count(with), VimMotion::line_end);
    if (!range.has_value())
    {
        return failed(cancelled(state), VimRepeatFailure::not_moved);
    }
    return performed(with, text, caret, range.value());
}
} // namespace detail
} // namespace nenenib::core
