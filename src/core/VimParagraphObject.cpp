#include "VimParagraphObject.hpp"

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "Selection.hpp"
#include "VimMode.hpp"
#include "VimMotionRange.hpp"
#include "VimRegisterKind.hpp"
#include "VimTextObjectCancel.hpp"
#include "VimTextObjectScope.hpp"
#include "VimTextObjectSpan.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
// 空白行の判定で一度に読む行頭からの窓の大きさ。行全体は写さない。
constexpr std::size_t window_bytes = 64;

[[nodiscard]] std::size_t line_of(const TextBuffer &text, Offset at)
{
    return text.position_of(at).line.value;
}

[[nodiscard]] bool blank_byte(char byte)
{
    return byte == ' ' || byte == '\t';
}

// Vim の linewhite。行が空か、space と tab だけ。最初の空白でない文字が見つかるまで窓を読む。
[[nodiscard]] bool line_is_white(const TextBuffer &text, std::size_t line)
{
    Offset at = text.line_start(LineNumber{line});
    const Offset end = text.line_end(LineNumber{line});
    while (at < end)
    {
        const Offset until{std::min(end.value, at.value + window_bytes)};
        if (!std::ranges::all_of(text.text_range(at, until), blank_byte))
        {
            return false;
        }
        at = until;
    }
    return true;
}

// 行の範囲を行単位のオペレータの範囲へ。
[[nodiscard]] VimMotionRange lines_between(const TextBuffer &text, std::size_t first,
                                           std::size_t last)
{
    return VimMotionRange{
        OffsetRange{text.line_start(LineNumber{first}), text.line_end(LineNumber{last})},
        VimRegisterKind::lines};
}

// ------------------------------------------------------------------ 選択の無い所から

// 同じ種類（空白行か段落の行か）の並びを後ろへ伸ばした最後の行。
[[nodiscard]] std::size_t through_run(const TextBuffer &text, std::size_t line, bool white)
{
    std::size_t end = line;
    while (end < text.line_count() && line_is_white(text, end + 1) == white)
    {
        ++end;
    }
    return end;
}

// 段落か空白行の並びの先頭の行。
[[nodiscard]] std::size_t run_start(const TextBuffer &text, std::size_t line, bool white)
{
    std::size_t start = line;
    while (start > 1 && line_is_white(text, start - 1) == white)
    {
        --start;
    }
    return start;
}

// 回数の 1 つぶん。i は並び 1 つ、a は段落とその後ろの空白行。空白行から始めた a の最後の
// 1 つは段落で止まる（後ろの空白行を足さない・実測）。
[[nodiscard]] std::size_t paragraph_step(const TextBuffer &text, std::size_t end, bool include,
                                         bool stops_after_text)
{
    const bool white_next = !include && line_is_white(text, end + 1);
    std::size_t grown = end;
    if (include || !white_next)
    {
        grown = through_run(text, grown + 1, false);
    }
    if (stops_after_text)
    {
        return grown;
    }
    if (include || white_next)
    {
        grown = through_run(text, grown, true);
    }
    return grown;
}

// 範囲の最後の行。回数の途中で本文が尽きたら範囲にならない（`d2ap` が最後の段落で何もしない）。
[[nodiscard]] std::optional<std::size_t> paragraph_end(const TextBuffer &text, std::size_t start,
                                                       bool include, std::size_t count)
{
    const bool white_in_front = line_is_white(text, start);
    std::size_t end = white_in_front ? through_run(text, start, true) : start - 1;
    std::size_t remaining = !include && white_in_front ? count - 1 : count;
    while (remaining > 0)
    {
        --remaining;
        if (end == text.line_count())
        {
            return std::nullopt;
        }
        end = paragraph_step(text, end, include, white_in_front && include && remaining == 0);
    }
    return end;
}

// a で後ろに空白行が無ければ、前の空白行を足す（空白行から始めたときは足さない・実測）。
[[nodiscard]] std::size_t leading_white_start(const TextBuffer &text, std::size_t start,
                                              std::size_t end, bool include)
{
    if (!include || line_is_white(text, start) || line_is_white(text, end))
    {
        return start;
    }
    std::size_t first = start;
    while (first > 1 && line_is_white(text, first - 1))
    {
        --first;
    }
    return first;
}

// ------------------------------------------------------------------ 選択を広げる

[[nodiscard]] std::size_t neighbor(std::size_t line, bool backward)
{
    return backward ? line - 1 : line + 1;
}

[[nodiscard]] bool at_boundary(const TextBuffer &text, std::size_t line, bool backward)
{
    return line == (backward ? 1 : text.line_count());
}

// 同じ種類の並びを向きの側へ伸ばした端の行。
[[nodiscard]] std::size_t run_edge(const TextBuffer &text, std::size_t line, bool backward)
{
    const bool white = line_is_white(text, line);
    std::size_t edge = line;
    while (!at_boundary(text, edge, backward) &&
           line_is_white(text, neighbor(edge, backward)) == white)
    {
        edge = neighbor(edge, backward);
    }
    return edge;
}

// 1 つぶん広げる。i は隣の並び 1 つ、a はその先の並びまで 2 つ。
[[nodiscard]] std::size_t extended_once(const TextBuffer &text, std::size_t line, bool backward,
                                        bool include)
{
    const std::size_t edge = run_edge(text, neighbor(line, backward), backward);
    if (!include || at_boundary(text, edge, backward))
    {
        return edge;
    }
    return run_edge(text, neighbor(edge, backward), backward);
}

// caret の行から選択を caret の向きへ広げる。本文の端で尽きたら取消だが、caret はそこまで動いて
// 行頭に載る。モードは変えない（文字単位 VISUAL のまま広がる）。どちらも実測。
[[nodiscard]] VimTextObjectOutcome extended_paragraphs(const TextBuffer &text,
                                                       const Selection &selection,
                                                       VimTextObjectRequest request,
                                                       std::size_t count)
{
    const std::size_t from = line_of(text, selection.caret);
    const bool backward = from < line_of(text, selection.anchor);
    const bool include = request.scope == VimTextObjectScope::around;
    std::size_t line = from;
    for (std::size_t step = 0; step < count; ++step)
    {
        if (at_boundary(text, line, backward))
        {
            return VimTextObjectCancel{
                Selection{selection.anchor, text.line_start(LineNumber{line})}};
        }
        line = extended_once(text, line, backward, include);
    }
    const Selection grown{selection.anchor, text.line_start(LineNumber{line})};
    const OffsetRange ordered = selection_range(grown);
    return VimTextObjectSpan{
        lines_between(text, line_of(text, ordered.begin), line_of(text, ordered.end)), grown,
        request.mode};
}
} // namespace

VimTextObjectOutcome vim_paragraph_object_range(const TextBuffer &text, const Selection &selection,
                                                VimTextObjectRequest request, std::size_t count)
{
    const std::size_t caret_line = line_of(text, selection.caret);
    const bool visual = is_visual_mode(request.mode);
    if (visual && caret_line != line_of(text, selection.anchor))
    {
        return extended_paragraphs(text, selection, request, count);
    }
    const bool include = request.scope == VimTextObjectScope::around;
    const std::size_t found = run_start(text, caret_line, line_is_white(text, caret_line));
    const auto end = paragraph_end(text, found, include, count);
    if (!end.has_value())
    {
        return VimTextObjectCancel{selection};
    }
    const std::size_t start = leading_white_start(text, found, end.value(), include);
    if (request.mode == VimMode::visual_line && start == caret_line)
    {
        // 行単位 VISUAL で範囲が caret の行から始まるなら、同じ範囲に留まらないよう選択を広げる
        // 形へ移る（`Vipip` が 1 行の段落で止まらないのはこれ。前の空白行を足した `ap` は
        // 広げない・実測）。
        return extended_paragraphs(text, selection, request, count);
    }
    const VimMotionRange range = lines_between(text, start, end.value());
    // anchor は行が変わるときだけ行頭へ動き、同じ行なら桁を保つ（`lvipo` の実測）。
    const Offset anchor =
        line_of(text, selection.anchor) == start ? selection.anchor : range.range.begin;
    return VimTextObjectSpan{range, Selection{anchor, text.line_start(LineNumber{end.value()})},
                             VimMode::visual_line};
}
} // namespace nenenib::core
