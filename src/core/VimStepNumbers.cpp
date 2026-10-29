#include "VimStepDetail.hpp"

#include "VimBlockEdit.hpp"
#include "VimBlockLine.hpp"
#include "VimNumber.hpp"
#include "VimReplaceBlock.hpp"
#include "VimReplaceRange.hpp"
#include "VimVisualRange.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::core
{
using detail::block_of;
using detail::cancelled;
using detail::count_of;
using detail::failed;
using detail::line_of;
using detail::restored_at;

namespace
{
// ---------------------------------------------------------------- 数の増減（ADR 0015 の決定 14）

// 最初に読む窓の片側の幅。数が窓の端まで続けば倍にして読み直す（行の全体は写さない）。
constexpr std::size_t number_window_bytes = 4096;

using NumberAt = std::expected<std::pair<Offset, VimNumberSpan>, VimNumberMiss>;

[[nodiscard]] bool subtracts(VimAction action) noexcept
{
    return action == VimAction::decrement_number || action == VimAction::decrement_sequence;
}

[[nodiscard]] bool in_sequence(VimAction action) noexcept
{
    return action == VimAction::increment_sequence || action == VimAction::decrement_sequence;
}

// 1 回に足す量。Vim は回数を 999999999 に貼り付ける。
[[nodiscard]] std::uint64_t step_of(const VimState &state) noexcept
{
    return std::min<std::uint64_t>(count_of(state.count), vim_number_count_limit);
}

[[nodiscard]] NumberAt placed(const std::expected<VimNumberSpan, VimNumberMiss> &found, Offset base)
{
    if (!found)
    {
        return std::unexpected(found.error());
    }
    return std::pair{base, found.value()};
}

// `from` の周りの窓（`looks_back` が偽なら `from` から右だけ）を読み、窓が足りなければ広げる。
// 返す Offset は窓の始まりで、span の位置はそこからのバイト数。
[[nodiscard]] NumberAt number_around(const TextBuffer &text, Offset from, bool looks_back,
                                     const auto &find)
{
    const LineNumber line = line_of(text, from);
    const Offset first = text.line_start(line);
    const Offset last = text.line_end(line);
    for (std::size_t reach = number_window_bytes;; reach *= 2)
    {
        const std::size_t back = looks_back ? std::min(reach, from.value - first.value) : 0;
        const Offset begin{from.value - back};
        const Offset end{std::min(last.value, from.value + reach)};
        const std::string bytes = text.text_range(begin, end);
        const VimNumberWindow window{bytes, back, begin == first, end == last};
        const std::expected<VimNumberSpan, VimNumberMiss> found = find(window);
        if (found || found.error() != VimNumberMiss::window_short)
        {
            return placed(found, begin);
        }
    }
}

[[nodiscard]] OffsetRange span_range(Offset base, const VimNumberSpan &span) noexcept
{
    return OffsetRange{Offset{base.value + span.begin}, Offset{base.value + span.end}};
}

// VISUAL の各行で数を探す範囲（Vim の op_addsub の pos.col と length）。文字単位は選択と行の
// 内容の重なり、行単位は行の全体、矩形は丸ごと入っている本文（block_prep の textcol / textlen）。
[[nodiscard]] std::vector<OffsetRange> selected_ranges(const VimState &state,
                                                       const VimEditorView &view)
{
    std::vector<OffsetRange> ranges;
    if (state.mode == VimMode::visual_block)
    {
        for (const VimBlockLine &line : block_of(state, view).lines)
        {
            ranges.push_back(line.inside);
        }
        return ranges;
    }
    const TextBuffer &text = view.text;
    const OffsetRange whole = vim_visual_range(text, view.selection, state.mode).range;
    const LineNumber last = line_of(text, whole.end);
    for (LineNumber line = line_of(text, whole.begin); line.value <= last.value; ++line.value)
    {
        const Offset begin{std::max(whole.begin.value, text.line_start(line).value)};
        const Offset end{std::min(whole.end.value, text.line_end(line).value)};
        ranges.push_back(OffsetRange{begin, std::max(begin, end)});
    }
    return ranges;
}

// VISUAL の後のキャレット（Vim の oap->start）。文字単位は選択の先頭、行単位は最初の行の行頭、
// 矩形は左上。u の戻り先も同じ所（実測）。
[[nodiscard]] Offset selection_start(const VimState &state, const VimEditorView &view)
{
    const Offset begin = selection_range(view.selection).begin;
    switch (state.mode)
    {
    case VimMode::visual_line:
        return view.text.line_start(line_of(view.text, begin));
    case VimMode::visual_block:
        return block_of(state, view).lines.front().range.begin;
    case VimMode::normal:
    case VimMode::insert:
    case VimMode::visual:
        return begin;
    }
    std::unreachable();
}
} // namespace

namespace detail
{
// NORMAL の `Ctrl-a` `Ctrl-x`。キャレットの上か右の数を 1 回の置き換えで増減し、キャレットは
// 数の最後の桁へ。レジスタは書かない。空行は失敗にせず何もしない（Vim の do_addsub の theend）。
// 数が無い行と `g Ctrl-a` は失敗（Vim のビープ）。
[[nodiscard]] VimStep numbered(const VimState &state, const VimEditorView &view, VimAction action)
{
    if (in_sequence(action))
    {
        return failed(cancelled(state), VimRepeatFailure::refused);
    }
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    const LineNumber line = line_of(text, caret);
    if (text.line_start(line) == text.line_end(line))
    {
        return VimStep{vim_resting_from(state, state.unnamed_register), VimNoEffect{}};
    }
    const NumberAt found = number_around(text, caret, true, [](const VimNumberWindow &window)
                                         { return vim_find_number(window); });
    if (!found)
    {
        return failed(cancelled(state), VimRepeatFailure::not_found);
    }
    const auto &[base, span] = found.value();
    const OffsetRange range = span_range(base, span);
    std::string body = vim_incremented_text(span, step_of(state), subtracts(action));
    const Offset landing{range.begin.value + body.size() - single_step};
    return VimStep{vim_resting_from(state, state.unnamed_register),
                   VimReplaceRange{range, std::move(body), landing}};
}

// VISUAL の `Ctrl-a` `Ctrl-x` と `g Ctrl-a` `g Ctrl-x`。各行の最初の数を増減し、全部を 1 つの
// 置き換えにまとめる（undo 1 単位）。`g` は数を変えた行ごとに足す量を回数ずつ増やす。数の無い
// 行は飛ばし、どの行にも数が無くても失敗にしない（Vim の op_addsub）。
[[nodiscard]] VimStep visual_numbered(const VimState &state, const VimEditorView &view,
                                      VimAction action)
{
    const std::uint64_t step = step_of(state);
    const bool bounded = state.mode != VimMode::visual_line;
    std::uint64_t amount = step;
    std::vector<VimBlockEdit> edits;
    for (const OffsetRange &selected : selected_ranges(state, view))
    {
        const std::size_t length = selected.end.value - selected.begin.value;
        const NumberAt found = number_around(
            view.text, selected.begin, false, [length, bounded](const VimNumberWindow &window)
            { return vim_find_selected_number(window, length, bounded); });
        if (!found)
        {
            continue;
        }
        const auto &[base, span] = found.value();
        edits.push_back(VimBlockEdit{span_range(base, span),
                                     vim_incremented_text(span, amount, subtracts(action))});
        amount += in_sequence(action) ? step : 0;
    }
    const Offset landing = selection_start(state, view);
    return restored_at(VimStep{vim_resting_from(state, state.unnamed_register),
                               VimReplaceBlock{std::move(edits), landing}},
                       landing);
}
} // namespace detail
} // namespace nenenib::core
