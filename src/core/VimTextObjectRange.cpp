#include "VimTextObjectRange.hpp"

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "Selection.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimBracketPair.hpp"
#include "VimMotionRange.hpp"
#include "VimRegisterKind.hpp"
#include "VimTextObject.hpp"
#include "VimTextObjectCancel.hpp"
#include "VimTextObjectScope.hpp"
#include "VimTextObjectSpan.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
constexpr std::uint32_t blank_group = 0;
constexpr char32_t escape_character = U'\\';

// ------------------------------------------------------------------ 位置の道具（Vim の inc / dec）

[[nodiscard]] LineNumber line_of(const TextBuffer &text, Offset at)
{
    return text.position_of(at).line;
}

// 行の中で次の code point。行の内容の終わり（Vim が NUL を置く桁）では動かない。
[[nodiscard]] Offset next_in_line(const TextBuffer &text, Offset at)
{
    const Offset end = text.line_end(line_of(text, at));
    if (!(at < end))
    {
        return at;
    }
    const std::string rest = text.text_range(at, end);
    return Offset{at.value + next_code_point(rest, Offset{0}).value};
}

[[nodiscard]] Offset previous_in_line(const TextBuffer &text, Offset at)
{
    const Offset start = text.line_start(line_of(text, at));
    if (!(start < at))
    {
        return at;
    }
    const std::string head = text.text_range(start, at);
    return Offset{start.value + previous_code_point(head, Offset{head.size()}).value};
}

// その位置の code point。行の内容の終わりは 0（Vim の NUL）。
[[nodiscard]] char32_t code_at(const TextBuffer &text, Offset at)
{
    const Offset end = text.line_end(line_of(text, at));
    if (!(at < end))
    {
        return 0;
    }
    return code_point_at(text.text_range(at, end), Offset{0});
}

[[nodiscard]] std::uint32_t class_at(const TextBuffer &text, Offset at, VimWordClass kind)
{
    const char32_t code = code_at(text, at);
    return code == 0 ? blank_group : vim_character_class(code, kind);
}

// 本文の終わり（最後の行の内容の終わり）。前向きの走査が尽きるのはここだけで、回数が尽きた
// `d9iw` のキャレットもここに残る（Vim 9.1 で実測・ADR 0031 の補足）。
[[nodiscard]] Offset buffer_end(const TextBuffer &text)
{
    return text.line_end(LineNumber{text.line_count()});
}

// 範囲の最後の文字の位置（VISUAL の caret になる）。行単位の範囲では最後の行の内容の終わり
// （Vim が NUL を置く桁）で、そこに caret を置くと選択が改行まで届く（実測）。
[[nodiscard]] Offset object_caret(const TextBuffer &text, const VimMotionRange &range)
{
    switch (range.kind)
    {
    case VimRegisterKind::uninitialized:
        // テキストオブジェクトの範囲は文字単位か行単位でだけ作られる。
        std::unreachable();
    case VimRegisterKind::lines:
        return range.range.end;
    case VimRegisterKind::characters:
        return is_empty(range.range) ? range.range.begin : previous_in_line(text, range.range.end);
    }
    std::unreachable();
}

// 範囲をそのまま選ぶ置き方。anchor は範囲の先頭、caret は最後の文字。
[[nodiscard]] Selection forward_selection(const TextBuffer &text, const VimMotionRange &range)
{
    return Selection{range.range.begin, object_caret(text, range)};
}

// Vim の inc()。行の内容の終わりへ、そこからは次の行の先頭へ。本文が尽きたら nullopt。
[[nodiscard]] std::optional<Offset> advanced(const TextBuffer &text, Offset at)
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
    return text.line_terminator_end(line);
}

// Vim の incl()。空でない行の内容の終わりは飛ばして、次の行の先頭まで進む。
[[nodiscard]] std::optional<Offset> advanced_to_text(const TextBuffer &text, Offset at)
{
    const auto next = advanced(text, at);
    if (!next.has_value())
    {
        return std::nullopt;
    }
    const LineNumber line = line_of(text, next.value());
    const bool at_line_end = next.value() == text.line_end(line);
    if (at_line_end && !(next.value() == text.line_start(line)))
    {
        return advanced(text, next.value());
    }
    return next;
}

// Vim の dec()。行の先頭からは前の行の内容の終わり（NUL の桁）へ。本文の先頭では nullopt。
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
    return text.line_end(LineNumber{line.value - 1});
}

// Vim の decl()。空でない行の内容の終わりは飛ばして、その行の最後の文字まで戻る。
[[nodiscard]] std::optional<Offset> retreated_to_text(const TextBuffer &text, Offset at)
{
    const auto back = retreated(text, at);
    if (!back.has_value())
    {
        return std::nullopt;
    }
    const LineNumber line = line_of(text, back.value());
    const bool at_line_end = back.value() == text.line_end(line);
    if (at_line_end && !(back.value() == text.line_start(line)))
    {
        return retreated(text, back.value());
    }
    return back;
}

// ------------------------------------------------------------------ 語（決定 2・決定 5）

[[nodiscard]] VimWordClass word_class_of(VimTextObject object)
{
    switch (object)
    {
    case VimTextObject::big_word:
        return VimWordClass::big_word;
    case VimTextObject::word:
    case VimTextObject::double_quote:
    case VimTextObject::single_quote:
    case VimTextObject::backtick:
    case VimTextObject::paren:
    case VimTextObject::brace:
    case VimTextObject::bracket:
    case VimTextObject::angle:
        return VimWordClass::word;
    }
    std::unreachable();
}

// Vim の back_in_line()。行の中だけで、同じ種類の並びの先頭へ戻る。
[[nodiscard]] Offset run_start_in_line(const TextBuffer &text, Offset at, VimWordClass kind)
{
    const std::uint32_t group = class_at(text, at, kind);
    Offset here = at;
    while (!(here == text.line_start(line_of(text, here))))
    {
        const Offset previous = previous_in_line(text, here);
        if (class_at(text, previous, kind) != group)
        {
            break;
        }
        here = previous;
    }
    return here;
}

// 1 単位ぶんの終わり（Vim の current_word の 1 周）。空白の上か語の上かと i / a の組で分かれる。
[[nodiscard]] std::optional<Offset> unit_end(const TextBuffer &text, Offset at,
                                             VimTextObjectRequest request)
{
    const VimWordClass kind = word_class_of(request.object);
    const bool include = request.scope == VimTextObjectScope::around;
    if ((class_at(text, at, kind) == blank_group) == include)
    {
        return vim_word_object_end(text, at, kind);
    }
    const Offset stop = vim_word_stop_forward(text, at, kind);
    const LineNumber line = line_of(text, stop);
    if (stop == text.line_start(line) && line.value > 1)
    {
        return text.line_end(LineNumber{line.value - 1});
    }
    return previous_in_line(text, stop);
}

// 1 単位ぶんの始まり（Vim の current_word の後ろ向きの 1 周）。前向きと同じ条件で 2 つに
// 分かれ、真なら連なりの先頭へ、偽なら手前の語の末尾の 1 つ後ろへ戻る（Issue #99 で実測）。
[[nodiscard]] std::optional<Offset> unit_begin(const TextBuffer &text, Offset at,
                                               VimTextObjectRequest request)
{
    const VimWordClass kind = word_class_of(request.object);
    const bool include = request.scope == VimTextObjectScope::around;
    if ((class_at(text, at, kind) == blank_group) == include)
    {
        return vim_word_object_begin(text, at, kind);
    }
    const auto end = vim_word_object_previous_end(text, at, kind);
    if (!end.has_value())
    {
        return std::nullopt;
    }
    return advanced_to_text(text, end.value());
}

// 2 つめ以降の単位。incl で 1 つ進めてから、同じ 1 周をもう一度（決定 5 の「積」）。
[[nodiscard]] std::optional<Offset> extended_end(const TextBuffer &text, Offset from,
                                                 VimTextObjectRequest request, std::size_t steps)
{
    Offset end = from;
    for (std::size_t step = 0; step < steps; ++step)
    {
        const auto next = advanced_to_text(text, end);
        if (!next.has_value())
        {
            return std::nullopt;
        }
        const auto grown = unit_end(text, next.value(), request);
        if (!grown.has_value())
        {
            return std::nullopt;
        }
        end = grown.value();
    }
    return end;
}

// 終わりの文字を含む文字単位の範囲。改行は越えない（オペレータの範囲は行の内容までで切れる）。
[[nodiscard]] VimMotionRange characters_through(const TextBuffer &text, Offset begin, Offset end)
{
    const Offset last = next_in_line(text, end);
    return VimMotionRange{OffsetRange{begin, last.value < begin.value ? begin : last},
                          VimRegisterKind::characters};
}

// aw で後ろの空白が取れなかったときだけ前の空白を足す。行頭まで空白だけなら足さない（実測）。
[[nodiscard]] Offset with_leading_blanks(const TextBuffer &text, Offset begin, Offset end,
                                         VimWordClass kind)
{
    if (class_at(text, end, kind) == blank_group)
    {
        return begin;
    }
    Offset here = begin;
    while (!(here == text.line_start(line_of(text, here))))
    {
        const Offset previous = previous_in_line(text, here);
        if (class_at(text, previous, kind) != blank_group)
        {
            return here;
        }
        here = previous;
    }
    return begin;
}

[[nodiscard]] VimMotionRange word_motion_range(const TextBuffer &text, Offset begin, Offset end,
                                               VimTextObjectRequest request)
{
    if (request.scope == VimTextObjectScope::inner)
    {
        return characters_through(text, begin, end);
    }
    return characters_through(
        text, with_leading_blanks(text, begin, end, word_class_of(request.object)), end);
}

// 選択が無いときの 1 つめの単位と、そこから回数ぶん。
[[nodiscard]] std::optional<Offset> fresh_word_end(const TextBuffer &text, Offset begin,
                                                   VimTextObjectRequest request, std::size_t count)
{
    const auto first = unit_end(text, begin, request);
    if (!first.has_value())
    {
        return std::nullopt;
    }
    return extended_end(text, first.value(), request, count - 1);
}

// 前向き（選択が無いか、caret が anchor 以上）。VISUAL で選択が 1 文字より大きいときは
// 始まりが選択の小さいほうのままで、caret 側だけ伸びる。
[[nodiscard]] VimTextObjectOutcome forward_word_outcome(const TextBuffer &text,
                                                        const Selection &selection,
                                                        VimTextObjectRequest request,
                                                        std::size_t count)
{
    const bool grows = has_selection(selection);
    const Offset begin = grows ? selection_range(selection).begin
                               : run_start_in_line(text, selection.caret,
                                                   word_class_of(request.object));
    const auto end = grows ? extended_end(text, selection.caret, request, count)
                           : fresh_word_end(text, begin, request, count);
    if (!end.has_value())
    {
        // 回数が本文で尽きた。Vim はそこまで作りかけた形を残してキャレットを本文の終わりへ
        // 運ぶ（`d9iw` が最後の文字に載るのはこれ・実測）。
        return VimTextObjectCancel{Selection{begin, buffer_end(text)}};
    }
    const VimMotionRange range = grows ? characters_through(text, begin, end.value())
                                       : word_motion_range(text, begin, end.value(), request);
    return VimTextObjectSpan{range, forward_selection(text, range)};
}

// 後ろ向きの選択（caret が anchor より小さい）。Vim は anchor を動かさず、caret だけを
// 1 単位ずつ手前へ運ぶ（`vhhiwiw` が語と空白を交互にさかのぼるのはこれ・Issue #99 で実測）。
// 本文の先頭で尽きたら取消で、そのときも caret は本文の先頭に残る。
[[nodiscard]] VimTextObjectOutcome backward_word_outcome(const TextBuffer &text,
                                                         const Selection &selection,
                                                         VimTextObjectRequest request,
                                                         std::size_t count)
{
    Offset caret = selection.caret;
    for (std::size_t step = 0; step < count; ++step)
    {
        const auto back = retreated_to_text(text, caret);
        if (!back.has_value())
        {
            return VimTextObjectCancel{Selection{selection.anchor, Offset{0}}};
        }
        const auto moved = unit_begin(text, back.value(), request);
        if (!moved.has_value())
        {
            return VimTextObjectCancel{Selection{selection.anchor, Offset{0}}};
        }
        caret = moved.value();
    }
    const Selection grown{selection.anchor, caret};
    return VimTextObjectSpan{VimMotionRange{selection_range(grown), VimRegisterKind::characters},
                             grown};
}

[[nodiscard]] VimTextObjectOutcome word_outcome(const TextBuffer &text, const Selection &selection,
                                                VimTextObjectRequest request, std::size_t count)
{
    if (selection.caret < selection.anchor)
    {
        return backward_word_outcome(text, selection, request, count);
    }
    return forward_word_outcome(text, selection, request, count);
}

// ------------------------------------------------------------------ 引用符（決定 6）

[[nodiscard]] char32_t quote_character(VimTextObject object)
{
    switch (object)
    {
    case VimTextObject::single_quote:
        return U'\'';
    case VimTextObject::backtick:
        return U'`';
    case VimTextObject::word:
    case VimTextObject::big_word:
    case VimTextObject::double_quote:
    case VimTextObject::paren:
    case VimTextObject::brace:
    case VimTextObject::bracket:
    case VimTextObject::angle:
        return U'"';
    }
    std::unreachable();
}

// 行の中の引用符の位置。`\` の直後の 1 文字は数えない（quoteescape の既定・実測）。
[[nodiscard]] std::vector<Offset> quotes_in(const std::string &content, char32_t quote)
{
    std::vector<Offset> found;
    Offset at{0};
    while (at.value < content.size())
    {
        const char32_t code = code_point_at(content, at);
        const Offset next = next_code_point(content, at);
        if (code == escape_character)
        {
            at = next.value < content.size() ? next_code_point(content, next) : next;
            continue;
        }
        if (code == quote)
        {
            found.push_back(at);
        }
        at = next;
    }
    return found;
}

// 対の開きになる引用符の番号。上に載っているなら行頭からの偶奇で、そうでなければ手前の 1 つ。
[[nodiscard]] std::optional<std::size_t> quote_start_index(const std::vector<Offset> &quotes,
                                                           Offset caret)
{
    if (quotes.empty())
    {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < quotes.size(); ++index)
    {
        if (quotes.at(index) == caret)
        {
            return index % 2 == 0 ? index : index - 1;
        }
    }
    std::size_t start = 0;
    for (std::size_t index = 0; index < quotes.size(); ++index)
    {
        if (quotes.at(index) < caret)
        {
            start = index;
        }
    }
    return start;
}

[[nodiscard]] std::optional<OffsetRange> quote_pair(const TextBuffer &text, Offset caret,
                                                    char32_t quote)
{
    const LineNumber line = line_of(text, caret);
    const Offset start = text.line_start(line);
    const std::vector<Offset> quotes =
        quotes_in(text.text_range(start, text.line_end(line)), quote);
    const auto index = quote_start_index(quotes, Offset{caret.value - start.value});
    if (!index.has_value() || index.value() + 1 >= quotes.size())
    {
        return std::nullopt;
    }
    return OffsetRange{Offset{start.value + quotes.at(index.value()).value},
                       Offset{start.value + quotes.at(index.value() + 1).value}};
}

[[nodiscard]] bool blank_at(const TextBuffer &text, Offset at)
{
    const char32_t code = code_at(text, at);
    return code == U' ' || code == U'\t';
}

// a" の空白。後ろに空白があればそちら、無ければ前の空白（どちらも行の中だけ・実測）。
[[nodiscard]] OffsetRange quote_with_blanks(const TextBuffer &text, const OffsetRange &pair)
{
    Offset end = next_in_line(text, pair.end);
    if (blank_at(text, end))
    {
        while (blank_at(text, end))
        {
            end = next_in_line(text, end);
        }
        return OffsetRange{pair.begin, end};
    }
    Offset begin = pair.begin;
    while (!(begin == text.line_start(line_of(text, begin))) &&
           blank_at(text, previous_in_line(text, begin)))
    {
        begin = previous_in_line(text, begin);
    }
    return OffsetRange{begin, end};
}

[[nodiscard]] std::optional<VimMotionRange> quote_range_at(const TextBuffer &text, Offset caret,
                                                           VimTextObjectRequest request,
                                                           std::size_t count)
{
    const auto pair = quote_pair(text, caret, quote_character(request.object));
    if (!pair.has_value())
    {
        return std::nullopt;
    }
    if (request.scope == VimTextObjectScope::around)
    {
        return VimMotionRange{quote_with_blanks(text, pair.value()), VimRegisterKind::characters};
    }
    if (count > 1)
    {
        return characters_through(text, pair.value().begin, pair.value().end);
    }
    return VimMotionRange{OffsetRange{next_in_line(text, pair.value().begin), pair.value().end},
                          VimRegisterKind::characters};
}

// ------------------------------------------------------------------ 括弧（決定 7）

[[nodiscard]] VimBracketPair bracket_pair_of(VimTextObject object)
{
    switch (object)
    {
    case VimTextObject::brace:
        return VimBracketPair{U'{', U'}'};
    case VimTextObject::bracket:
        return VimBracketPair{U'[', U']'};
    case VimTextObject::angle:
        return VimBracketPair{U'<', U'>'};
    case VimTextObject::word:
    case VimTextObject::big_word:
    case VimTextObject::double_quote:
    case VimTextObject::single_quote:
    case VimTextObject::backtick:
    case VimTextObject::paren:
        return VimBracketPair{U'(', U')'};
    }
    std::unreachable();
}

// 括弧を 1 つ見て深さを更新する。対応の取れていない開き（後ろ向きの走査）なら真。
[[nodiscard]] bool opens_the_block(char32_t code, VimBracketPair pair, std::size_t &depth)
{
    if (code == pair.close)
    {
        ++depth;
        return false;
    }
    if (code != pair.open || depth == 0)
    {
        return code == pair.open;
    }
    --depth;
    return false;
}

// 対応の取れていない閉じ（前向きの走査）なら真。
[[nodiscard]] bool closes_the_block(char32_t code, VimBracketPair pair, std::size_t &depth)
{
    if (code == pair.open)
    {
        ++depth;
        return false;
    }
    if (code != pair.close || depth == 0)
    {
        return code == pair.close;
    }
    --depth;
    return false;
}

// 1 行ぶんを後ろへ。対応の取れていない開きが見つかればその位置（深さは呼び出し元に残る）。
[[nodiscard]] std::optional<Offset> scanned_back(const std::string &content, VimBracketPair pair,
                                                 std::size_t &depth)
{
    Offset at{content.size()};
    while (at.value > 0)
    {
        at = previous_code_point(content, at);
        if (opens_the_block(code_point_at(content, at), pair, depth))
        {
            return at;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Offset> unmatched_open_backward(const TextBuffer &text, Offset at,
                                                            VimBracketPair pair)
{
    std::size_t depth = 0;
    LineNumber line = line_of(text, at);
    Offset limit = at;
    for (;;)
    {
        const Offset start = text.line_start(line);
        const auto found = scanned_back(text.text_range(start, limit), pair, depth);
        if (found.has_value())
        {
            return Offset{start.value + found.value().value};
        }
        if (line.value <= 1)
        {
            return std::nullopt;
        }
        line = LineNumber{line.value - 1};
        limit = text.line_end(line);
    }
}

// 1 行ぶんを前へ。開きでも閉じでも、最初に現れた括弧の位置。
[[nodiscard]] std::optional<Offset> first_bracket(const std::string &content, VimBracketPair pair)
{
    Offset at{0};
    while (at.value < content.size())
    {
        const char32_t code = code_point_at(content, at);
        if (code == pair.open || code == pair.close)
        {
            return at;
        }
        at = next_code_point(content, at);
    }
    return std::nullopt;
}

// 中に居ないときは、前にある次の塊を使う。先に対応の取れていない閉じが来たら取消（実測）。
[[nodiscard]] std::optional<Offset> next_open_forward(const TextBuffer &text, Offset at,
                                                      VimBracketPair pair)
{
    LineNumber line = line_of(text, at);
    Offset from = next_in_line(text, at);
    for (;;)
    {
        const auto found = first_bracket(text.text_range(from, text.line_end(line)), pair);
        if (found.has_value())
        {
            const Offset here{from.value + found.value().value};
            return code_at(text, here) == pair.open ? std::optional<Offset>{here} : std::nullopt;
        }
        if (line.value >= text.line_count())
        {
            return std::nullopt;
        }
        line = LineNumber{line.value + 1};
        from = text.line_start(line);
    }
}

[[nodiscard]] std::optional<Offset> scanned_forward(const std::string &content, VimBracketPair pair,
                                                    std::size_t &depth)
{
    Offset at{0};
    while (at.value < content.size())
    {
        if (closes_the_block(code_point_at(content, at), pair, depth))
        {
            return at;
        }
        at = next_code_point(content, at);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Offset> matched_close_forward(const TextBuffer &text, Offset open_at,
                                                          VimBracketPair pair)
{
    std::size_t depth = 0;
    LineNumber line = line_of(text, open_at);
    Offset from = next_in_line(text, open_at);
    for (;;)
    {
        const auto found = scanned_forward(text.text_range(from, text.line_end(line)), pair, depth);
        if (found.has_value())
        {
            return Offset{from.value + found.value().value};
        }
        if (line.value >= text.line_count())
        {
            return std::nullopt;
        }
        line = LineNumber{line.value + 1};
        from = text.line_start(line);
    }
}

// 段の数だけ開きをたどる。中に居れば 1 段ずつ外へ、居なければ前にある塊へ同じ数だけ入る。
// 向きは最初の 1 回で決まり、外へ向かった走査は途中で前へ折り返さない（Vim 9.1 で実測）。
[[nodiscard]] std::optional<Offset> block_open_for(const TextBuffer &text, Offset caret,
                                                   VimBracketPair pair, std::size_t count)
{
    const Offset start = code_at(text, caret) == pair.open ? next_in_line(text, caret) : caret;
    const auto behind = unmatched_open_backward(text, start, pair);
    const bool outward = behind.has_value();
    std::optional<Offset> open = outward ? behind : next_open_forward(text, start, pair);
    for (std::size_t step = 1; step < count && open.has_value(); ++step)
    {
        open = outward ? unmatched_open_backward(text, open.value(), pair)
                       : next_open_forward(text, open.value(), pair);
    }
    return open;
}

// i( の 2 つの寄せ。開きが行の最後なら中は次の行から、閉じの前が空白だけなら中は前の行までで、
// 両方が起きたときだけ行単位になる（Vim 9.1 で実測）。
[[nodiscard]] bool opens_at_line_end(const TextBuffer &text, Offset open_at)
{
    return next_in_line(text, open_at) == text.line_end(line_of(text, open_at));
}

[[nodiscard]] bool closes_after_blanks(const TextBuffer &text, Offset close_at, Offset open_at)
{
    const LineNumber line = line_of(text, close_at);
    if (line.value <= line_of(text, open_at).value)
    {
        return false;
    }
    Offset at = text.line_start(line);
    while (at < close_at)
    {
        if (!blank_at(text, at))
        {
            return false;
        }
        at = next_in_line(text, at);
    }
    return true;
}

[[nodiscard]] VimMotionRange inner_block_range(const TextBuffer &text, const OffsetRange &block)
{
    const bool from_next_line = opens_at_line_end(text, block.begin);
    const bool to_previous_line = closes_after_blanks(text, block.end, block.begin);
    const Offset begin = from_next_line ? text.line_terminator_end(line_of(text, block.begin))
                                        : next_in_line(text, block.begin);
    const Offset last = to_previous_line
                            ? text.line_end(LineNumber{line_of(text, block.end).value - 1})
                            : previous_in_line(text, block.end);
    if (last.value < begin.value)
    {
        return VimMotionRange{OffsetRange{begin, begin}, VimRegisterKind::characters};
    }
    if (from_next_line && to_previous_line)
    {
        return VimMotionRange{
            OffsetRange{text.line_start(line_of(text, begin)), text.line_end(line_of(text, last))},
            VimRegisterKind::lines};
    }
    return characters_through(text, begin, last);
}

[[nodiscard]] std::optional<VimMotionRange> block_range_at(const TextBuffer &text, Offset caret,
                                                           VimTextObjectRequest request,
                                                           std::size_t count)
{
    const VimBracketPair pair = bracket_pair_of(request.object);
    const auto open = block_open_for(text, caret, pair, count);
    if (!open.has_value())
    {
        return std::nullopt;
    }
    const auto close = matched_close_forward(text, open.value(), pair);
    if (!close.has_value())
    {
        return std::nullopt;
    }
    const OffsetRange block{open.value(), close.value()};
    if (request.scope == VimTextObjectScope::around)
    {
        return characters_through(text, block.begin, block.end);
    }
    return inner_block_range(text, block);
}

// ------------------------------------------------------------------ VISUAL の広げ方（決定 4）

// 新しい範囲が今の選択より広くないか（Vim の current_block / current_quote の「もう 1 段外へ」）。
[[nodiscard]] bool within_selection(const TextBuffer &text, const Selection &selection,
                                    const VimMotionRange &range)
{
    const OffsetRange ordered = selection_range(selection);
    return range.range.begin.value >= ordered.begin.value &&
           object_caret(text, range).value <= ordered.end.value;
}

// 向きを保つ置き方（引用符）。後ろ向きの選択では caret が範囲の先頭に残る（Issue #99 で実測。
// 括弧は同じ形でも必ず前向きになるので、そちらは forward_selection を使う）。
[[nodiscard]] Selection kept_direction(const TextBuffer &text, const Selection &selection,
                                       const VimMotionRange &range)
{
    if (selection.caret < selection.anchor)
    {
        return Selection{object_caret(text, range), range.range.begin};
    }
    return forward_selection(text, range);
}

[[nodiscard]] bool quoted(VimTextObject object)
{
    switch (object)
    {
    case VimTextObject::double_quote:
    case VimTextObject::single_quote:
    case VimTextObject::backtick:
        return true;
    case VimTextObject::word:
    case VimTextObject::big_word:
    case VimTextObject::paren:
    case VimTextObject::brace:
    case VimTextObject::bracket:
    case VimTextObject::angle:
        return false;
    }
    std::unreachable();
}

[[nodiscard]] std::optional<VimMotionRange>
pair_range_at(const TextBuffer &text, Offset caret, VimTextObjectRequest request, std::size_t count)
{
    return quoted(request.object) ? quote_range_at(text, caret, request, count)
                                  : block_range_at(text, caret, request, count);
}

// 引用符と括弧は選択より広くならなければもう 1 段外へ（`vi(i(` / `vi"i"` の実測）。
[[nodiscard]] std::optional<VimMotionRange> paired_range(const TextBuffer &text,
                                                         const Selection &selection,
                                                         VimTextObjectRequest request,
                                                         std::size_t count)
{
    const Offset from = selection_range(selection).begin;
    const auto first = pair_range_at(text, from, request, count);
    if (!has_selection(selection) || !first.has_value() ||
        !within_selection(text, selection, first.value()))
    {
        return first;
    }
    return pair_range_at(text, from, request, count + 1);
}

// 対が見つからなければ取消で、キャレットも選択も動かない（Vim 9.1 で実測）。
[[nodiscard]] VimTextObjectOutcome paired_outcome(const TextBuffer &text,
                                                  const Selection &selection,
                                                  VimTextObjectRequest request, std::size_t count)
{
    const auto range = paired_range(text, selection, request, count);
    if (!range.has_value())
    {
        return VimTextObjectCancel{selection};
    }
    const Selection placed = quoted(request.object)
                                 ? kept_direction(text, selection, range.value())
                                 : forward_selection(text, range.value());
    return VimTextObjectSpan{range.value(), placed};
}
} // namespace

VimTextObjectOutcome vim_text_object_range(const TextBuffer &text, const Selection &selection,
                                           VimTextObjectRequest request, std::size_t count)
{
    switch (request.object)
    {
    case VimTextObject::word:
    case VimTextObject::big_word:
        return word_outcome(text, selection, request, count);
    case VimTextObject::double_quote:
    case VimTextObject::single_quote:
    case VimTextObject::backtick:
    case VimTextObject::paren:
    case VimTextObject::brace:
    case VimTextObject::bracket:
    case VimTextObject::angle:
        return paired_outcome(text, selection, request, count);
    }
    std::unreachable();
}
} // namespace nenenib::core
