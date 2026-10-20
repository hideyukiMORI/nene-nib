#include "VimTagObject.hpp"

#include "AsciiCase.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "Selection.hpp"
#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimMode.hpp"
#include "VimMotionRange.hpp"
#include "VimRegisterKind.hpp"
#include "VimTextObjectCancel.hpp"
#include "VimTextObjectScope.hpp"
#include "VimTextObjectSpan.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
// 一度に読む窓の大きさ。読む位置を中心に取り直す。行全体は写さない。
constexpr std::size_t window_bytes = 4096;
// 1 文字を歩く窓の最初の大きさ。結合文字の列が長ければ倍にして読み直す。
constexpr std::size_t character_window_bytes = 64;
constexpr std::size_t code_point_bytes = 4;
// Vim の vim_iswordc（utf_class が 2 以上なら語の文字）。
constexpr std::uint32_t word_class = 2;
constexpr std::size_t no_limit = std::numeric_limits<std::size_t>::max();
// 開きタグの名前を終わらせるバイト（Vim の `<[^ \t>/!]\+`）。
constexpr std::string_view name_stops = " \t>/!";

// 本文と、いま読んでいる窓（中身と先頭の offset）。
using Reader = std::tuple<const TextBuffer &, std::string, std::size_t>;
// 照合の 1 つ。並びでは先頭の offset、照合の答えでは終わりの offset と、開きタグか。
using Hit = std::pair<std::size_t, bool>;
// 見つかったタグの塊。開きの `<` と閉じの `<` の offset。
using Block = std::pair<std::size_t, std::size_t>;
// 走査の始まり（Vim の cursor）と、元の選択の両端（Vim の old_start / old_end）。
using Start = std::tuple<std::size_t, std::size_t, std::size_t>;

[[nodiscard]] const TextBuffer &text_of(const Reader &reader)
{
    return std::get<0>(reader);
}

[[nodiscard]] std::size_t size_of(const Reader &reader)
{
    return text_of(reader).size_bytes();
}

// at のバイト。本文の外は 0 で、どのタグの記号にも当たらない。
[[nodiscard]] char byte_at(Reader &reader, std::size_t at)
{
    auto &[text, bytes, base] = reader;
    if (at >= text.size_bytes())
    {
        return '\0';
    }
    if (at < base || at - base >= bytes.size())
    {
        base = at - std::min(at, window_bytes / 2);
        bytes =
            text.text_range(Offset{base}, Offset{std::min(text.size_bytes(), base + window_bytes)});
    }
    return bytes.at(at - base);
}

[[nodiscard]] std::size_t line_of(const Reader &reader, std::size_t at)
{
    return text_of(reader).position_of(Offset{at}).line.value;
}

[[nodiscard]] bool blank_byte(char byte)
{
    return byte == ' ' || byte == '\t';
}

// 改行の始まりか。CRLF の本文では LF の直前の CR も改行（TextBuffer::line_end と同じ模型）。
[[nodiscard]] bool newline_at(Reader &reader, std::size_t at)
{
    const char byte = byte_at(reader, at);
    return byte == '\n' || (byte == '\r' && text_of(reader).line_ending() == LineEnding::crlf &&
                            byte_at(reader, at + 1) == '\n');
}

// Vim の `\_s`（space・tab・改行）の 1 文字か。
[[nodiscard]] bool space_at(Reader &reader, std::size_t at)
{
    return blank_byte(byte_at(reader, at)) || newline_at(reader, at);
}

// `\_s` の 1 文字の後ろ。改行は CRLF でも 1 文字。
[[nodiscard]] std::size_t past_space(Reader &reader, std::size_t at)
{
    return byte_at(reader, at) == '\r' ? at + 2 : at + 1;
}

[[nodiscard]] std::optional<std::size_t> next_byte(Reader &reader, std::size_t from, char wanted)
{
    for (std::size_t at = from; at < size_of(reader); ++at)
    {
        if (byte_at(reader, at) == wanted)
        {
            return at;
        }
    }
    return std::nullopt;
}

// ------------------------------------------------------------------ 1 文字を歩く（ADR 0053）

// 窓の末尾の欠けた code point を落とす。
[[nodiscard]] std::size_t whole_code_points(std::string_view window)
{
    std::size_t size = window.size();
    while (size > 0 && (static_cast<unsigned char>(window.at(size - 1)) & 0xC0U) == 0x80U)
    {
        --size;
    }
    return size > 0 ? size - 1 : 0;
}

// 窓の先頭の続きのバイトを飛ばした数。
[[nodiscard]] std::size_t leading_continuations(std::string_view window)
{
    std::size_t skipped = 0;
    while (skipped < window.size() &&
           (static_cast<unsigned char>(window.at(skipped)) & 0xC0U) == 0x80U)
    {
        ++skipped;
    }
    return skipped;
}

// 行の中で次の文字の先頭。at は行の内容の終わりより前。
[[nodiscard]] std::size_t next_in_line(const TextBuffer &text, std::size_t at)
{
    const std::size_t end = text.line_end(text.position_of(Offset{at}).line).value;
    for (std::size_t width = character_window_bytes;; width *= 2)
    {
        const std::size_t until = std::min(end, at + width);
        const std::string window = text.text_range(Offset{at}, Offset{until});
        const std::string_view whole =
            until == end ? std::string_view{window}
                         : std::string_view{window}.substr(0, whole_code_points(window));
        const Offset next = vim_character_end(whole, Offset{0});
        if (until == end || next.value < whole.size())
        {
            return at + next.value;
        }
    }
}

// 行の中で手前の文字の先頭。at は行頭より後ろ。
[[nodiscard]] std::size_t previous_in_line(const TextBuffer &text, std::size_t at)
{
    const std::size_t start = text.line_start(text.position_of(Offset{at}).line).value;
    for (std::size_t width = character_window_bytes;; width *= 2)
    {
        const std::size_t from = at - std::min(at - start, width);
        const std::string window = text.text_range(Offset{from}, Offset{at});
        const std::size_t skipped = from == start ? 0 : leading_continuations(window);
        const std::string_view whole = std::string_view{window}.substr(skipped);
        const Offset found = vim_character_start(whole, Offset{whole.size()});
        if (from == start || found.value > 0)
        {
            return from + skipped + found.value;
        }
    }
}

// Vim の inc() の答えの位置。行の内容の終わり（NUL の桁）へ、そこからは次の行の先頭へ。
[[nodiscard]] std::size_t advanced(const TextBuffer &text, std::size_t at)
{
    const LineNumber line = text.position_of(Offset{at}).line;
    if (at < text.line_end(line).value)
    {
        return next_in_line(text, at);
    }
    return line.value < text.line_count() ? text.line_start(LineNumber{line.value + 1}).value : at;
}

// Vim の dec()。行の先頭からは前の行の内容の終わり（NUL の桁）へ。本文の先頭では動かない。
[[nodiscard]] std::size_t retreated(const TextBuffer &text, std::size_t at)
{
    const LineNumber line = text.position_of(Offset{at}).line;
    if (text.line_start(line).value < at)
    {
        return previous_in_line(text, at);
    }
    return line.value > 1 ? text.line_end(LineNumber{line.value - 1}).value : at;
}

// Vim の decl()。行をまたいで空でない行の終わりに着いたら、その行の最後の文字まで戻る。
[[nodiscard]] std::size_t retreated_to_text(const TextBuffer &text, std::size_t at)
{
    const std::size_t back = retreated(text, at);
    const LineNumber line = text.position_of(Offset{back}).line;
    const bool crossed = text.position_of(Offset{at}).line.value != line.value;
    if (crossed && text.line_start(line).value < back)
    {
        return previous_in_line(text, back);
    }
    return back;
}

// ------------------------------------------------------------------ タグの照合（Vim の正規表現）

// 開きタグの名前の終わり（`<[^ \t>/!]\+` の名前・改行で止まる）。
[[nodiscard]] std::size_t name_end(Reader &reader, std::size_t from)
{
    std::size_t at = from;
    while (at < size_of(reader) && !newline_at(reader, at) &&
           !name_stops.contains(byte_at(reader, at)))
    {
        ++at;
    }
    return at;
}

// 空白の後ろの属性の終わり（`\_[^>]\{-}[^/]>`）。最初の `>` の手前の 1 文字が `/` でなければ
// そこで終わる。手前の 1 文字が改行でよいのは名前つきの開き（`\_[^/]`）だけ。
[[nodiscard]] std::optional<std::size_t> attributes_end(Reader &reader, std::size_t from,
                                                        bool newline_before_close)
{
    const auto close = next_byte(reader, from, '>');
    if (!close.has_value())
    {
        return std::nullopt;
    }
    const std::size_t angle = close.value();
    if (angle > from)
    {
        const char before = byte_at(reader, angle - 1);
        if (before != '/' && (newline_before_close || before != '\n'))
        {
            return angle + 1;
        }
    }
    if (byte_at(reader, angle + 1) == '>')
    {
        return angle + 2;
    }
    return std::nullopt;
}

// 空白 1 つまでの後ろの `>`（`\_s\=>`）。
[[nodiscard]] std::optional<std::size_t> closed_after(Reader &reader, std::size_t at)
{
    const std::size_t from = space_at(reader, at) ? past_space(reader, at) : at;
    if (byte_at(reader, from) == '>' && from < size_of(reader))
    {
        return from + 1;
    }
    return std::nullopt;
}

// 後ろへ探す開きタグ `<[^ \t>/!]\+\%(\_s\_[^>]\{-}[^/]>\|$\|\_s\=>\)` の終わり。
[[nodiscard]] std::optional<std::size_t> open_tag_end(Reader &reader, std::size_t at)
{
    const std::size_t name = name_end(reader, at + 1);
    if (name == at + 1)
    {
        return std::nullopt;
    }
    if (space_at(reader, name))
    {
        const auto attributes = attributes_end(reader, past_space(reader, name), false);
        if (attributes.has_value())
        {
            return attributes;
        }
    }
    if (name >= size_of(reader) || newline_at(reader, name))
    {
        return name;
    }
    return closed_after(reader, name);
}

// 後ろへ探す閉じタグ `</[^>]*>` の終わり（行をまたがない）。
[[nodiscard]] std::optional<std::size_t> close_tag_end(Reader &reader, std::size_t at)
{
    if (byte_at(reader, at + 1) != '/')
    {
        return std::nullopt;
    }
    for (std::size_t from = at + 2; from < size_of(reader) && !newline_at(reader, from); ++from)
    {
        if (byte_at(reader, from) == '>')
        {
            return from + 1;
        }
    }
    return std::nullopt;
}

// 名前が at から始まるか（ASCII の大文字小文字を畳む・Vim の `\c`）。
[[nodiscard]] bool name_matches(Reader &reader, std::size_t at, std::string_view name)
{
    for (std::size_t index = 0; index < name.size(); ++index)
    {
        if (ascii_folded(byte_at(reader, at + index)) != ascii_folded(name.at(index)))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool word_code(char32_t code)
{
    return vim_character_class(code, VimWordClass::word) >= word_class;
}

// 名前の後ろが語の終わり（Vim の `\>`）か。名前の最後の文字が語の文字で、次が語の文字でない。
[[nodiscard]] bool word_ends(Reader &reader, std::string_view name, std::size_t at)
{
    const Offset last = previous_code_point(name, Offset{name.size()});
    if (!word_code(code_point_at(name, last)))
    {
        return false;
    }
    if (at >= size_of(reader) || newline_at(reader, at))
    {
        return true;
    }
    const TextBuffer &text = text_of(reader);
    const std::string next =
        text.text_range(Offset{at}, Offset{std::min(text.size_bytes(), at + code_point_bytes)});
    return !word_code(code_point_at(next, Offset{0}));
}

// 前へ探す名前つきの開きタグ `<name\>\%(\_s\_[^>]\{-}\_[^/]>\|\_s\?>\)\c` の終わり。
[[nodiscard]] std::optional<std::size_t> named_open_end(Reader &reader, std::size_t at,
                                                        std::string_view name)
{
    const std::size_t after = at + 1 + name.size();
    if (!name_matches(reader, at + 1, name) || !word_ends(reader, name, after))
    {
        return std::nullopt;
    }
    if (space_at(reader, after))
    {
        const auto attributes = attributes_end(reader, past_space(reader, after), true);
        if (attributes.has_value())
        {
            return attributes;
        }
    }
    return closed_after(reader, after);
}

// 前へ探す名前つきの閉じタグ `</name>\c` の終わり。
[[nodiscard]] std::optional<std::size_t> named_close_end(Reader &reader, std::size_t at,
                                                         std::string_view name)
{
    const std::size_t angle = at + 2 + name.size();
    if (byte_at(reader, at + 1) != '/' || !name_matches(reader, at + 2, name) ||
        byte_at(reader, angle) != '>')
    {
        return std::nullopt;
    }
    return angle + 1;
}

// at の `<` から始まる照合の終わりと開きか。name が空なら後ろへ探す形、あれば前へ探す形。
// 開きと閉じは `/` の有無で排他なので、選択の順は Vim の `\(start\)\|\(end\)` と同じになる。
[[nodiscard]] std::optional<Hit> hit_at(Reader &reader, std::size_t at, std::string_view name)
{
    const auto open = name.empty() ? open_tag_end(reader, at) : named_open_end(reader, at, name);
    if (open.has_value())
    {
        return Hit{open.value(), true};
    }
    const auto close = name.empty() ? close_tag_end(reader, at) : named_close_end(reader, at, name);
    if (close.has_value())
    {
        return Hit{close.value(), false};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> angle_in(Reader &reader, std::size_t from,
                                                  std::size_t stop)
{
    for (std::size_t at = from; at < stop; ++at)
    {
        if (byte_at(reader, at) == '<')
        {
            return at;
        }
    }
    return std::nullopt;
}

// 行の照合の並び（Vim の searchit が 'cpoptions' の c の下で行頭から数える並び）。次の照合は前の
// 照合の終わりから探し、行をまたいだ照合の後ろは数えない。limit 以上で始まる照合は要らない。
[[nodiscard]] std::vector<Hit> line_hits(Reader &reader, LineNumber line, std::size_t limit,
                                         std::string_view name)
{
    const TextBuffer &text = text_of(reader);
    const std::size_t stop = std::min(text.line_end(line).value, limit);
    const std::size_t line_stop = text.line_end(line).value;
    std::vector<Hit> hits;
    std::size_t from = text.line_start(line).value;
    for (auto angle = angle_in(reader, from, stop); angle.has_value();
         angle = angle_in(reader, from, stop))
    {
        const auto hit = hit_at(reader, angle.value(), name);
        from = hit.has_value() ? hit.value().first : angle.value() + 1;
        if (hit.has_value())
        {
            hits.emplace_back(angle.value(), hit.value().second);
        }
        if (from > line_stop)
        {
            break;
        }
    }
    return hits;
}

// ------------------------------------------------------------------ searchpair

// 並びを後ろから数え、開きが数え尽くした所。
[[nodiscard]] std::optional<std::size_t> settled_backward(const std::vector<Hit> &hits,
                                                          std::size_t &nest)
{
    for (const auto &[at, opens] : std::views::reverse(hits))
    {
        nest = opens ? nest - 1 : nest + 1;
        if (nest == 0)
        {
            return at;
        }
    }
    return std::nullopt;
}

// from より前の未対応の開きタグ（名前を問わず入れ子を数える・本文の先頭で折り返さない）。
[[nodiscard]] std::optional<std::size_t> unmatched_open_before(Reader &reader, std::size_t from)
{
    std::size_t nest = 1;
    std::size_t limit = from;
    for (std::size_t line = line_of(reader, from); line >= 1; --line)
    {
        const auto found = settled_backward(line_hits(reader, LineNumber{line}, limit, {}), nest);
        if (found.has_value())
        {
            return found;
        }
        limit = no_limit;
    }
    return std::nullopt;
}

// 並びを前から数え、閉じが数え尽くした所。open 以前で始まる照合は数えない。
[[nodiscard]] std::optional<std::size_t> settled_forward(const std::vector<Hit> &hits,
                                                         std::size_t open, std::size_t &nest)
{
    for (const auto &[at, opens] : hits)
    {
        if (at <= open)
        {
            continue;
        }
        nest = opens ? nest + 1 : nest - 1;
        if (nest == 0)
        {
            return at;
        }
    }
    return std::nullopt;
}

// open の開きタグと対になる閉じタグ（同じ名前の開きを数える・本文の終わりで折り返さない）。
[[nodiscard]] std::optional<std::size_t> matched_close_after(Reader &reader, std::size_t open,
                                                             std::string_view name)
{
    std::size_t nest = 1;
    const std::size_t lines = text_of(reader).line_count();
    for (std::size_t line = line_of(reader, open); line <= lines; ++line)
    {
        const auto found =
            settled_forward(line_hits(reader, LineNumber{line}, no_limit, name), open, nest);
        if (found.has_value())
        {
            return found;
        }
    }
    return std::nullopt;
}

// 開きタグの名前（`<` の次から行の終わり・`>`・空白の手前まで）。
[[nodiscard]] std::string tag_name(Reader &reader, std::size_t open)
{
    std::string name;
    for (std::size_t at = open + 1; at < size_of(reader) && !newline_at(reader, at); ++at)
    {
        const char byte = byte_at(reader, at);
        if (byte == '>' || blank_byte(byte))
        {
            break;
        }
        name.push_back(byte);
    }
    return name;
}

// 外側へ count 個目の開きタグ。
[[nodiscard]] std::optional<std::size_t> outer_open(Reader &reader, std::size_t from,
                                                    std::size_t count)
{
    std::optional<std::size_t> open = from;
    for (std::size_t step = 0; step < count && open.has_value(); ++step)
    {
        open = unmatched_open_before(reader, open.value());
    }
    return open;
}

// 塊（Vim の again: の繰り返し）。閉じが無いか元の終わりより前なら、1 つ外の開きで探し直す。
[[nodiscard]] std::optional<Block> tag_block(Reader &reader, std::size_t from, std::size_t count,
                                             std::size_t old_end)
{
    for (auto open = outer_open(reader, from, count); open.has_value();
         open = outer_open(reader, open.value(), 1))
    {
        const std::string name = tag_name(reader, open.value());
        if (name.empty())
        {
            return std::nullopt;
        }
        const auto close = matched_close_after(reader, open.value(), name);
        if (close.has_value() && close.value() >= old_end)
        {
            return Block{open.value(), close.value()};
        }
    }
    return std::nullopt;
}

// ------------------------------------------------------------------ 走査の始まり

// キャレットの下か手前の `<`（同じ行の中だけ・間に `>` があれば無い・Vim の in_html_tag）。
[[nodiscard]] std::optional<std::size_t> angle_under(Reader &reader, std::size_t at)
{
    if (byte_at(reader, at) == '<')
    {
        return at;
    }
    const std::size_t start = text_of(reader).line_start(LineNumber{line_of(reader, at)}).value;
    for (std::size_t back = at; back > start;)
    {
        --back;
        const char byte = byte_at(reader, back);
        if (byte == '>')
        {
            return std::nullopt;
        }
        if (byte == '<')
        {
            return back;
        }
    }
    return std::nullopt;
}

// 開きタグの上か（`/` で閉じる `<x/>` は開きでない）。
[[nodiscard]] bool on_open_tag(Reader &reader, std::optional<std::size_t> angle)
{
    if (!angle.has_value() || byte_at(reader, angle.value() + 1) == '/')
    {
        return false;
    }
    const auto close = next_byte(reader, angle.value() + 1, '>');
    return close.has_value() &&
           !(close.value() > angle.value() + 1 && byte_at(reader, close.value() - 1) == '/');
}

// 行頭の空白の上なら最初の空白でない文字（空白だけの行なら内容の終わり）へ（Vim の inindent）。
[[nodiscard]] std::size_t past_indent(Reader &reader, std::size_t at)
{
    const TextBuffer &text = text_of(reader);
    const LineNumber line{line_of(reader, at)};
    std::size_t first = text.line_start(line).value;
    const std::size_t end = text.line_end(line).value;
    while (first < end && blank_byte(byte_at(reader, first)))
    {
        ++first;
    }
    return std::max(at, first);
}

// 畳んだ位置（NORMAL と 1 文字の選択）。開きタグの上ならその `>` へ、閉じタグの上ならその手前へ
// 動き、閉じタグの手前が元の終わりになる。
[[nodiscard]] Start collapsed_start(Reader &reader, std::size_t caret, std::size_t old_end)
{
    const std::size_t at = past_indent(reader, caret);
    const auto angle = angle_under(reader, at);
    if (on_open_tag(reader, angle))
    {
        return Start{next_byte(reader, at, '>').value_or(at), caret, old_end};
    }
    if (angle.has_value() && byte_at(reader, angle.value() + 1) == '/')
    {
        const std::size_t before = retreated(text_of(reader), angle.value());
        return Start{before, caret, before};
    }
    return Start{at, caret, old_end};
}

// 走査の始まりと元の両端。NORMAL の元の終わりはキャレットの 1 つ手前（Vim の decl）。広げる
// VISUAL は選択の小さいほうの端から探す。
[[nodiscard]] Start search_start(Reader &reader, const Selection &selection, bool visual)
{
    const std::size_t caret = selection.caret.value;
    if (!visual || !has_selection(selection))
    {
        return collapsed_start(reader, caret,
                               visual ? caret : retreated_to_text(text_of(reader), caret));
    }
    const OffsetRange ordered = selection_range(selection);
    return Start{ordered.begin.value, ordered.begin.value, ordered.end.value};
}

// ------------------------------------------------------------------ 範囲

// it の両端（Vim の start_pos と end_pos）。始まりは開きの後ろの最初の `>` の次（無ければ開きの
// まま）、終わりは閉じの `<` の 1 つ手前。
[[nodiscard]] std::pair<std::size_t, std::size_t> inner_ends(Reader &reader, const Block &block)
{
    const auto angle = next_byte(reader, block.first + 1, '>');
    const std::size_t begin =
        angle.has_value() ? advanced(text_of(reader), angle.value()) : block.first;
    return {begin, retreated(text_of(reader), block.second)};
}

// it。閉じの `<` の手前まで。終わりが始まりより前なら空の範囲で、VISUAL は始まりの 1 文字。
[[nodiscard]] VimTextObjectSpan inner_span(const Block &block,
                                           std::pair<std::size_t, std::size_t> ends)
{
    const auto [begin, last] = ends;
    const std::size_t end = std::max(begin, block.second);
    const std::size_t caret = last < begin ? begin : last;
    return VimTextObjectSpan{
        VimMotionRange{OffsetRange{Offset{begin}, Offset{end}}, VimRegisterKind::characters},
        Selection{Offset{begin}, Offset{caret}}, VimMode::visual};
}

// at。開きの `<` から閉じの `>` まで。
[[nodiscard]] VimTextObjectSpan around_span(Reader &reader, const Block &block)
{
    const std::size_t angle = next_byte(reader, block.second, '>').value_or(block.second);
    const std::size_t end = advanced(text_of(reader), angle);
    return VimTextObjectSpan{
        VimMotionRange{OffsetRange{Offset{block.first}, Offset{end}}, VimRegisterKind::characters},
        Selection{Offset{block.first}, Offset{angle}}, VimMode::visual};
}
} // namespace

VimTextObjectOutcome vim_tag_object_range(const TextBuffer &text, const Selection &selection,
                                          VimTextObjectRequest request, std::size_t count)
{
    Reader reader{text, std::string{}, 0};
    const bool visual = is_visual_mode(request.mode);
    const auto [from, old_start, old_end] = search_start(reader, selection, visual);
    const auto block = tag_block(reader, from, count, old_end);
    if (!block.has_value())
    {
        return VimTextObjectCancel{selection};
    }
    if (request.scope == VimTextObjectScope::around)
    {
        return around_span(reader, block.value());
    }
    const auto ends = inner_ends(reader, block.value());
    if (!visual || ends != std::pair{old_start, old_end})
    {
        return inner_span(block.value(), ends);
    }
    // VISUAL で今の選択と同じ範囲になったら at で選び直す（`vitit` は同じ塊の at・実測）。
    const auto whole = tag_block(reader, old_start, count, old_end);
    if (!whole.has_value())
    {
        return VimTextObjectCancel{selection};
    }
    return around_span(reader, whole.value());
}
} // namespace nenenib::core
