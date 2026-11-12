#include "VimSentenceObject.hpp"

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "Selection.hpp"
#include "Utf8.hpp"
#include "VimCaret.hpp"
#include "VimMode.hpp"
#include "VimMotionRange.hpp"
#include "VimParagraphStart.hpp"
#include "VimRegisterKind.hpp"
#include "VimTextObjectScope.hpp"
#include "VimTextObjectSpan.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
constexpr std::u32string_view sentence_ends = U".!?";
constexpr std::u32string_view closers = U")]\"'";
constexpr std::u32string_view punctuation = U".!?)]\"'";

// 進めた位置と、本文の終わりで止められなかったか（Vim の incl が -1 を返しても位置は動き得る）。
using Advance = std::pair<Offset, bool>;
// 選択を広げる走査の 2 つの位置（Vim の pos と cursor）。
using Walk = std::pair<Offset, Offset>;
// 広げる並びの数（as は 2 倍済み）と、as か。
using Units = std::pair<std::size_t, bool>;

[[nodiscard]] bool listed(std::u32string_view set, char32_t code)
{
    for (const char32_t member : set)
    {
        if (member == code)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool blank(char32_t code)
{
    return code == U' ' || code == U'\t';
}

[[nodiscard]] LineNumber line_of(const TextBuffer &text, Offset at)
{
    return text.position_of(at).line;
}

[[nodiscard]] Offset end_of_line(const TextBuffer &text, Offset at)
{
    return text.line_end(line_of(text, at));
}

[[nodiscard]] bool line_empty(const TextBuffer &text, LineNumber line)
{
    return text.line_start(line) == text.line_end(line);
}

// その位置の code point（Vim の gchar_pos）。行の内容の終わりは 0（Vim の NUL）。
[[nodiscard]] char32_t code_at(const TextBuffer &text, Offset at)
{
    return vim_code_point_at(text, at, end_of_line(text, at));
}

// 行の中で次の文字の先頭（ADR 0053）。at は行の内容の終わりより前。
[[nodiscard]] Offset next_in_line(const TextBuffer &text, Offset at)
{
    return vim_character_after(text, at, end_of_line(text, at));
}

// 行の中で手前の文字の先頭。at は行頭より後ろ。
[[nodiscard]] Offset previous_in_line(const TextBuffer &text, Offset at)
{
    return vim_character_before(text, at, text.line_start(line_of(text, at)));
}

// ------------------------------------------------------------------ Vim の inc / dec / incl / decl

[[nodiscard]] std::optional<Offset> stepped(const TextBuffer &text, Offset at)
{
    const LineNumber line = line_of(text, at);
    if (at < text.line_end(line))
    {
        return next_in_line(text, at);
    }
    if (line.value >= text.line_count())
    {
        return std::nullopt;
    }
    return text.line_start(LineNumber{line.value + 1});
}

// 空でない行の内容の終わりは飛ばして次の行の先頭へ。
[[nodiscard]] Advance advanced(const TextBuffer &text, Offset at)
{
    const auto next = stepped(text, at);
    if (!next.has_value())
    {
        return Advance{at, false};
    }
    const bool onto_end = at < end_of_line(text, at) && next.value() == end_of_line(text, at);
    if (!onto_end)
    {
        return Advance{next.value(), true};
    }
    const auto after = stepped(text, next.value());
    return after.has_value() ? Advance{after.value(), true} : Advance{next.value(), false};
}

// 空でない行の内容の終わりは飛ばしてその行の最後の文字へ。本文の先頭では動かない。
[[nodiscard]] std::optional<Offset> retreated(const TextBuffer &text, Offset at)
{
    const LineNumber line = line_of(text, at);
    if (text.line_start(line) < at)
    {
        return previous_in_line(text, at);
    }
    if (line.value <= 1)
    {
        return std::nullopt;
    }
    const LineNumber above{line.value - 1};
    return line_empty(text, above) ? text.line_start(above)
                                   : previous_in_line(text, text.line_end(above));
}

[[nodiscard]] Offset retreated_or_kept(const TextBuffer &text, Offset at)
{
    return retreated(text, at).value_or(at);
}

[[nodiscard]] Offset advanced_past_blanks(const TextBuffer &text, Offset at)
{
    Offset here = at;
    while (blank(code_at(text, here)))
    {
        const Advance next = advanced(text, here);
        here = next.first;
        if (!next.second)
        {
            break;
        }
    }
    return here;
}

// Vim の find_first_blank。手前の空白の並びの最初の空白へ戻る。
[[nodiscard]] Offset first_blank(const TextBuffer &text, Offset at)
{
    Offset here = at;
    for (auto back = retreated(text, here); back.has_value(); back = retreated(text, here))
    {
        here = back.value();
        if (!blank(code_at(text, here)))
        {
            return advanced(text, here).first;
        }
    }
    return here;
}

// ------------------------------------------------------------------ 文の境（Vim の findsent）

// 向きへ 1 文字。本文の端で止められたら nullopt。行の内容の終わりから歩くときだけ使うので、
// 止められたのに位置が動く Vim の incl の端（最後の行の最後の文字から）はここに来ない。
[[nodiscard]] std::optional<Offset> moved(const TextBuffer &text, Offset at, bool forward)
{
    if (!forward)
    {
        return retreated(text, at);
    }
    const Advance next = advanced(text, at);
    return next.second ? std::optional<Offset>{next.first} : std::nullopt;
}

// 空の行（か行の内容の終わり）の上なら、文字のある所まで歩く。
[[nodiscard]] Offset past_empty(const TextBuffer &text, Offset at, bool forward)
{
    Offset here = at;
    for (auto next = moved(text, here, forward); next.has_value();
         next = moved(text, here, forward))
    {
        here = next.value();
        if (code_at(text, here) != 0)
        {
            break;
        }
    }
    return here;
}

// 戻ってよい句読点か（閉じ括弧類は手前も句読点のときだけ越える）。
[[nodiscard]] bool crosses_punctuation(char32_t code, char32_t before, bool found_dot)
{
    if (found_dot)
    {
        return false;
    }
    return !listed(closers, code) || listed(punctuation, before);
}

// 手前の空白と句読点でない文字まで戻る。
[[nodiscard]] Offset back_over_punctuation(const TextBuffer &text, Offset at, bool forward)
{
    Offset here = at;
    bool found_dot = false;
    for (char32_t code = code_at(text, here); blank(code) || listed(punctuation, code);
         code = code_at(text, here))
    {
        const auto before = retreated(text, here);
        if (!before.has_value() || (forward && line_empty(text, line_of(text, before.value()))) ||
            !crosses_punctuation(code, code_at(text, before.value()), found_dot))
        {
            break;
        }
        found_dot = listed(sentence_ends, code);
        here = before.value();
    }
    return here;
}

// `.` `!` `?` の後の閉じ括弧類の先が空白か行末なら、文の終わりの次の位置。
[[nodiscard]] std::optional<Offset> after_sentence_end(const TextBuffer &text, Offset at)
{
    Offset here = next_in_line(text, at);
    while (listed(closers, code_at(text, here)))
    {
        here = next_in_line(text, here);
    }
    const char32_t code = code_at(text, here);
    if (!blank(code) && code != 0)
    {
        return std::nullopt;
    }
    return code == 0 ? stepped(text, here).value_or(here) : here;
}

// 行頭が段落の始まり（Vim の findsent の `pos.col == 0 && startPS(pos.lnum, NUL,
// FALSE)`・#1359）か。
// 行頭かは直前の 1 バイトで見る（1 歩ごとに行番号を問わない・巨大な 1 行の歩き #751 の費用）。
[[nodiscard]] bool at_paragraph_start(const TextBuffer &text, Offset at)
{
    if (at.value > 0 && text.text_range(Offset{at.value - 1}, at) != "\n")
    {
        return false;
    }
    return vim_paragraph_start(text, line_of(text, at));
}

// その位置で文が終わるなら、文の終わりの次の位置。空の行と段落の始まりの行頭（nroff のマクロの
// 行・改ページ）でも終わり、後ろ向きに別の行のそこへ着いたらその次の行の先頭になる。
[[nodiscard]] std::optional<Offset> sentence_stop(const TextBuffer &text, Offset at, bool forward,
                                                  LineNumber start_line)
{
    const char32_t code = code_at(text, at);
    if (code == 0 || at_paragraph_start(text, at))
    {
        const LineNumber line = line_of(text, at);
        const bool below = !forward && !(line == start_line);
        return below ? text.line_start(LineNumber{line.value + 1}) : at;
    }
    if (!listed(sentence_ends, code))
    {
        return std::nullopt;
    }
    return after_sentence_end(text, at);
}

// 文が止まり得る文字のバイト（`.` `!` `?` と行の内容の終わりと同じに読まれる NUL）。どれも ASCII
// で UTF-8 の多バイトの中に現れず、幅 0 でもないので、見つけた所は必ず Vim の 1 文字の先頭。
[[nodiscard]] bool stop_byte(char byte)
{
    return byte == '.' || byte == '!' || byte == '?' || byte == '\0';
}

// 本文の塊 segment（本文の base から）のうち span と重なる所で、向きの最初の止まり得るバイト。
// 重ならなければ nullopt。
[[nodiscard]] std::optional<std::size_t> stop_in_segment(std::string_view segment, std::size_t base,
                                                         OffsetRange span, bool forward)
{
    const std::size_t begin = std::max(span.begin.value, base);
    const std::size_t end = std::min(span.end.value, base + segment.size());
    for (std::size_t step = 0; begin + step < end; ++step)
    {
        const std::size_t at = forward ? begin + step : end - 1 - step;
        if (stop_byte(segment.at(at - base)))
        {
            return at;
        }
    }
    return std::nullopt;
}

// span の中で向きの最初の止まり得るバイトの位置。本文の塊を写さずにバイトで走査する（#751）。
[[nodiscard]] std::optional<Offset> stop_between(const TextBuffer &text, OffsetRange span,
                                                 bool forward)
{
    std::optional<std::size_t> found;
    std::size_t base = 0;
    for (const std::string_view segment : text.segments())
    {
        const auto in_segment = stop_in_segment(segment, base, span, forward);
        found = in_segment.has_value() ? in_segment : found;
        if ((forward && found.has_value()) || span.end.value <= base + segment.size())
        {
            break;
        }
        base += segment.size();
    }
    return found.has_value() ? std::optional<Offset>{Offset{found.value()}} : std::nullopt;
}

// 1 文字ずつ歩いても文が止まらない所を飛ばした位置。行の中の次の止まり得る文字へ、無ければ
// 前向きは行の最後の文字・後ろ向きは行頭へ。行をまたぐ歩き（空の行の判定）は 1 文字ずつのまま。
[[nodiscard]] Offset skipped_to_stop(const TextBuffer &text, Offset here, bool forward)
{
    const LineNumber line = line_of(text, here);
    if (!forward)
    {
        const Offset start = text.line_start(line);
        return stop_between(text, OffsetRange{start, here}, false).value_or(start);
    }
    const Offset end = text.line_end(line);
    if (!(here < end))
    {
        return here;
    }
    const auto stop = stop_between(text, OffsetRange{Offset{here.value + 1}, end}, true);
    if (stop.has_value())
    {
        return stop.value();
    }
    const Offset last = previous_in_line(text, end);
    return here < last ? last : here;
}

// Vim の incl / decl を向きで選ぶ。2 つめは本文の端で止められなかったか。
[[nodiscard]] Advance stepped_toward(const TextBuffer &text, Offset at, bool forward)
{
    if (forward)
    {
        return advanced(text, at);
    }
    return Advance{retreated_or_kept(text, at), at.value > 0};
}

// 文の終わりを探す。2 つめは本文の端で尽きたか（そのときは後ろの空白を飛ばさない）。
[[nodiscard]] Advance sentence_edge(const TextBuffer &text, Offset at, bool forward)
{
    const LineNumber start_line = line_of(text, at);
    Offset here = at;
    for (;;)
    {
        const auto stop = sentence_stop(text, here, forward, start_line);
        if (stop.has_value())
        {
            return Advance{stop.value(), true};
        }
        // 止まらない文字の並びは塊の走査で飛ばす（結果は 1 文字ずつ歩いたのと同じ・#751）。
        const Offset skipped = skipped_to_stop(text, here, forward);
        if (!(skipped == here))
        {
            here = skipped;
            continue;
        }
        const Advance next = stepped_toward(text, here, forward);
        if (!next.second)
        {
            return Advance{next.first, false};
        }
        here = next.first;
    }
}

// Vim の findsent の回数 1 つぶん。前向きは次の文の先頭、後ろ向きは今の（か手前の）文の先頭。
// 2 つめは文の終わりを探す歩きが本文の端で尽きなかったか（Vim は残りの回数があれば FAIL）。
// 前向きに段落の始まり（空でない行）の行頭から歩くと次の行の先頭へ進み、最後の行なら回数に
// よらず nullopt（Vim の `return FAIL`・動かない・#1359）。
[[nodiscard]] std::optional<Advance> sentence_walk(const TextBuffer &text, Offset at, bool forward)
{
    Offset here = at;
    if (code_at(text, here) == 0)
    {
        here = past_empty(text, here, forward);
        if (forward)
        {
            return Advance{advanced_past_blanks(text, here), true};
        }
    }
    else if (forward && at_paragraph_start(text, here))
    {
        const LineNumber line = line_of(text, here);
        if (line.value >= text.line_count())
        {
            return std::nullopt;
        }
        return Advance{advanced_past_blanks(text, text.line_start(LineNumber{line.value + 1})),
                       true};
    }
    else if (!forward)
    {
        here = retreated_or_kept(text, here);
    }
    const Advance edge = sentence_edge(text, back_over_punctuation(text, here, forward), forward);
    return edge.second ? Advance{advanced_past_blanks(text, edge.first), true} : edge;
}

// 端で尽きた周の位置。最後の周なら成功（Vim の findsent の break）、残りがあれば失敗（FAIL）。
[[nodiscard]] std::optional<Offset> kept_if_last(Offset at, bool last)
{
    return last ? std::optional<Offset>{at} : std::nullopt;
}

// 移動の回数 1 つぶん（Vim の findsent の while の 1 周）。動かなかったら 1 文字進めて同じ周を
// やり直し、進めなければ残りの回数があれば失敗（textobject.c の EQUAL_POS(prev_pos, pos) の
// 枝）。周は位置が進むか失敗で抜けるので、本文の端に着いた後の回数を空回りしない（巨大な回数で
// 固まらない・#1334）。last は最後の周か。
[[nodiscard]] std::optional<Offset> sentence_turn(const TextBuffer &text, Offset at, bool forward,
                                                  bool last)
{
    Offset here = at;
    for (;;)
    {
        const auto walk = sentence_walk(text, here, forward);
        if (!walk.has_value())
        {
            return std::nullopt;
        }
        const Advance walked = walk.value();
        if (!walked.second)
        {
            return kept_if_last(walked.first, last);
        }
        if (!(walked.first == here))
        {
            return walked.first;
        }
        const Advance stepped = stepped_toward(text, here, forward);
        if (!stepped.second)
        {
            return kept_if_last(stepped.first, last);
        }
        here = stepped.first;
    }
}

// Vim の findsent(dir, 1)。FAIL ならキャレットは動かない。周の終わりに動いていなければ 1 文字
// 進めてやり直す枝（EQUAL_POS(prev_pos, pos)）も通る: 前の行が文の終わりで、行頭が `.` `!` `?`
// や閉じの記号の行から前へ探すと、その記号は前の文の終わりに数えられて行頭へ戻り、動かないので
// 次の文の先頭まで進む（`is` `as` が失敗しない・#1508）。
[[nodiscard]] Offset sentence_start(const TextBuffer &text, Offset at, bool forward)
{
    return sentence_turn(text, at, forward, true).value_or(at);
}

// 周の始めの位置が 2 周前（同じ向きの印）と同じなら、残りの周は 2 周の繰り返しなので、残りを
// 同じ偶奇の 1 か 2 に縮める。周の中身は位置と印だけで決まり、最後の周だけが違う。本文の端に
// 着いた後の巨大な回数の `is` `as` を空回りしない（Vim の findsent_forward
// は回数ぶん回る・#1334）。
[[nodiscard]] std::size_t shortened_left(std::array<std::optional<Offset>, 2> &seen, Offset here,
                                         bool starts, std::size_t left)
{
    std::optional<Offset> &last = seen.at(starts ? 1U : 0U);
    const bool repeats = last.has_value() && last.value() == here;
    last = here;
    return repeats ? 1U + ((left - 1U) % 2U) : left;
}

// Vim の findsent_forward。文の先頭から、並びの数だけ前へ。
[[nodiscard]] Offset sentences_forward(const TextBuffer &text, Offset at, std::size_t count,
                                       bool at_start)
{
    Offset here = at;
    bool starts = at_start;
    std::array<std::optional<Offset>, 2> seen{};
    for (std::size_t left = count; left > 0; --left)
    {
        left = shortened_left(seen, here, starts, left);
        here = sentence_start(text, here, true);
        if (starts)
        {
            here = first_blank(text, here);
        }
        if (left == 1 || starts)
        {
            here = retreated_or_kept(text, here);
        }
        starts = !starts;
    }
    return here;
}

// ------------------------------------------------------------------ 選択を広げる

// 文の中に居るか（from から until の手前までに空白でない文字があるか）。
[[nodiscard]] bool inside_sentence(const TextBuffer &text, Offset from, Offset until)
{
    for (Offset pos = from; pos < until; pos = advanced(text, pos).first)
    {
        if (!blank(code_at(text, pos)))
        {
            return true;
        }
    }
    return false;
}

// 後ろ向きに並びの数だけ広げる。文の先頭と空白の並びを交互にさかのぼる。
[[nodiscard]] Offset sentences_backward(const TextBuffer &text, Offset at, Units units,
                                        bool at_start)
{
    Offset cursor = at;
    bool starts = at_start;
    std::array<std::optional<Offset>, 2> seen{};
    for (std::size_t left = units.first; left > 0; --left)
    {
        left = shortened_left(seen, cursor, starts, left);
        cursor = starts ? first_blank(text, cursor) : cursor;
        if (!starts || (!units.second && !blank(code_at(text, cursor))))
        {
            cursor = sentence_start(text, cursor, false);
        }
        starts = !starts;
    }
    return cursor;
}

// caret が anchor より手前の選択。caret の 1 つ手前から次の文の先頭までに空白でない文字が
// あれば文の中と見る（空白の並びの最初の空白に caret があると、その並びは選んだ済みとして
// 手前の文へ進む・実測）。
[[nodiscard]] Offset extended_backward(const TextBuffer &text, Offset start, Walk walk, Units units)
{
    Offset cursor = walk.second;
    bool at_start = !inside_sentence(text, retreated_or_kept(text, walk.first), cursor);
    if (!at_start)
    {
        cursor = sentence_start(text, cursor, false);
        at_start = cursor == start;
        cursor = at_start ? cursor : sentence_start(text, cursor, true);
    }
    return sentences_backward(text, cursor, units, at_start);
}

// caret が anchor 以上の選択。
[[nodiscard]] Offset extended_forward(const TextBuffer &text, Offset start, Walk walk, Units units)
{
    const Offset pos = advanced(text, walk.first).first;
    Offset cursor = walk.second;
    bool at_start = true;
    if (!(pos == cursor))
    {
        at_start = inside_sentence(text, pos, cursor);
        cursor = at_start ? sentence_start(text, cursor, false) : start;
    }
    return sentences_forward(text, cursor, units.first, at_start);
}

// 選択を caret の向きへ広げる。anchor とモードは変えない（行単位 VISUAL のまま・実測）。
[[nodiscard]] Selection extended_sentences(const TextBuffer &text, const Selection &selection,
                                           Walk walk, Units units)
{
    const Offset start = selection.caret;
    const Offset cursor = start < selection.anchor ? extended_backward(text, start, walk, units)
                                                   : extended_forward(text, start, walk, units);
    return Selection{selection.anchor, cursor};
}

[[nodiscard]] VimTextObjectSpan selected_span(const Selection &selection, VimMode mode)
{
    return VimTextObjectSpan{
        VimMotionRange{selection_range(selection), VimRegisterKind::characters}, selection, mode};
}

// ------------------------------------------------------------------ 選択の無い所から

// as のとき、後ろの空白が取れなければ前の空白を足す（空白から始めたら後ろの空白を落とす）。
[[nodiscard]] Selection with_around_blanks(const TextBuffer &text, Selection span, bool start_blank)
{
    if (start_blank)
    {
        const Offset back = first_blank(text, span.caret);
        return Selection{span.anchor,
                         blank(code_at(text, back)) ? retreated_or_kept(text, back) : back};
    }
    if (!blank(code_at(text, span.caret)))
    {
        return Selection{first_blank(text, span.anchor), span.caret};
    }
    return span;
}

// 両端が含まれる文の範囲（anchor が先頭・caret が最後の文字）。
[[nodiscard]] Selection sentence_span(const TextBuffer &text, Walk walk,
                                      VimTextObjectRequest request, std::size_t count)
{
    const auto [caret, next] = walk;
    const bool include = request.scope == VimTextObjectScope::around;
    const bool start_blank = advanced_past_blanks(text, caret) == next;
    const Offset start = start_blank ? first_blank(text, caret) : sentence_start(text, next, false);
    const Offset from = start_blank ? next : start;
    const std::size_t units = include ? count * 2 : count - (start_blank ? 1 : 0);
    const Offset end =
        units > 0 ? sentences_forward(text, from, units, true) : retreated_or_kept(text, from);
    const Selection span{start, end};
    return include ? with_around_blanks(text, span, start_blank) : span;
}

[[nodiscard]] bool only_blanks_before(const TextBuffer &text, Offset at)
{
    for (Offset pos = text.line_start(line_of(text, at)); pos < at; pos = next_in_line(text, pos))
    {
        if (!blank(code_at(text, pos)))
        {
            return false;
        }
    }
    return true;
}

// オペレータの範囲。文の後ろに文字があれば exclusive で次の位置まで（文の最後の文字の後ろが改行
// なら次の行の先頭まで）、本文の終わりなら inclusive。次の行の先頭で終わる exclusive は Vim の
// 規則で前の行の終わりへ戻り、始まりの前が空白だけなら行単位になる（`dis` が行を消す・実測）。
[[nodiscard]] VimMotionRange operator_range(const TextBuffer &text, const Selection &span)
{
    const Advance after = advanced(text, span.caret);
    if (!after.second)
    {
        return VimMotionRange{
            OffsetRange{span.anchor, text.line_end(LineNumber{text.line_count()})},
            VimRegisterKind::characters};
    }
    const LineNumber end_line = line_of(text, after.first);
    const bool wraps = after.first == text.line_start(end_line) &&
                       line_of(text, span.anchor).value < end_line.value;
    if (!wraps)
    {
        return VimMotionRange{OffsetRange{span.anchor, after.first}, VimRegisterKind::characters};
    }
    const LineNumber last{end_line.value - 1};
    if (only_blanks_before(text, span.anchor))
    {
        return VimMotionRange{
            OffsetRange{text.line_start(line_of(text, span.anchor)), text.line_end(last)},
            VimRegisterKind::lines};
    }
    return VimMotionRange{OffsetRange{span.anchor, text.line_end(last)},
                          VimRegisterKind::characters};
}

} // namespace

std::optional<Offset> vim_sentence_motion(const TextBuffer &text, Offset caret, bool forward,
                                          std::size_t count)
{
    Offset here = caret;
    for (std::size_t left = count; left > 0; --left)
    {
        const auto turned = sentence_turn(text, here, forward, left == 1);
        if (!turned.has_value())
        {
            return std::nullopt;
        }
        here = turned.value();
    }
    return here;
}

VimTextObjectOutcome vim_sentence_object_range(const TextBuffer &text, const Selection &selection,
                                               VimTextObjectRequest request, std::size_t count)
{
    const bool include = request.scope == VimTextObjectScope::around;
    const Offset caret = selection.caret;
    const Offset next = sentence_start(text, caret, true);
    const bool visual = is_visual_mode(request.mode);
    const Units units{include ? count * 2 : count, include};
    if (visual && has_selection(selection))
    {
        return selected_span(extended_sentences(text, selection, Walk{caret, next}, units),
                             request.mode);
    }
    const Selection span = sentence_span(text, Walk{caret, next}, request, count);
    if (!visual)
    {
        return VimTextObjectSpan{operator_range(text, span), span, VimMode::visual};
    }
    if (span.anchor == span.caret)
    {
        // 文の前の 1 つの空白に is を使うと同じ所に留まるので、広げる形へ移る（Vim と同じ）。
        return selected_span(extended_sentences(text, Selection{selection.anchor, span.anchor},
                                                Walk{advanced_past_blanks(text, caret), span.caret},
                                                units),
                             request.mode);
    }
    return selected_span(span, VimMode::visual);
}
} // namespace nenenib::core
