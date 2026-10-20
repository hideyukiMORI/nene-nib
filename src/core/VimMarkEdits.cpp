#include "VimMarkEdits.hpp"

#include "LineNumber.hpp"
#include "VimCaret.hpp"
#include "VimMarkShift.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
constexpr std::size_t one_line = 1;

[[nodiscard]] std::optional<std::size_t> named_index(char32_t name) noexcept
{
    if (name < U'a' || name > U'z')
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(name - U'a');
}

// Vim の setmark_pos が受けるが、この版が持たない名前（ADR 0152 の限界）。
[[nodiscard]] bool kept_unset(char32_t name) noexcept
{
    const bool upper = name >= U'A' && name <= U'Z';
    const bool digit = name >= U'0' && name <= U'9';
    return upper || digit || name == U'"' || name == U'[' || name == U']' || name == U'<' ||
           name == U'>';
}

[[nodiscard]] bool is_context(char32_t name) noexcept
{
    return name == U'\'' || name == U'`';
}

// 走査は自前で書く（STL の検索は core の外へ memchr を出す・ARC-003）。
[[nodiscard]] std::size_t newlines_in(std::string_view text) noexcept
{
    std::size_t count = 0;
    for (const char byte : text)
    {
        count += static_cast<std::size_t>(byte == '\n');
    }
    return count;
}

[[nodiscard]] bool blank_only(std::string_view text) noexcept
{
    for (const char byte : text)
    {
        if (byte != ' ' && byte != '\t')
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool ends_with_newline(std::string_view text) noexcept
{
    return !text.empty() && text.back() == '\n';
}

// 改行で始まるか（LF と CRLF）。
[[nodiscard]] bool starts_with_newline(std::string_view text) noexcept
{
    return text.starts_with('\n') || text.starts_with("\r\n");
}

[[nodiscard]] LineNumber line_after(LineNumber line, std::size_t amount) noexcept
{
    return LineNumber{line.value + amount};
}

[[nodiscard]] LineNumber line_before(LineNumber line, std::size_t amount) noexcept
{
    return LineNumber{line.value - amount};
}

// 消した本文と入れた本文の共通の頭と尾を除いた編集（行を丸ごと書き直す J gJ の置換を、改行と
// 字下げだけの削除として読むため）。キャレットの戻り先は使わない。
[[nodiscard]] Edit narrowed(const Edit &edit)
{
    const std::string_view removed = edit.removed;
    const std::string_view inserted = edit.inserted;
    std::size_t head = 0;
    while (head < removed.size() && head < inserted.size() && removed[head] == inserted[head])
    {
        ++head;
    }
    std::size_t tail = 0;
    while (tail < removed.size() - head && tail < inserted.size() - head &&
           removed[removed.size() - tail - 1] == inserted[inserted.size() - tail - 1])
    {
        ++tail;
    }
    return Edit{Offset{edit.at.value + head},
                std::string{removed.substr(head, removed.size() - head - tail)},
                std::string{inserted.substr(head, inserted.size() - head - tail)}, edit.restore};
}

// J gJ（1 つの改行と次の行の字下げだけを消した）か。edit は narrowed の後。
[[nodiscard]] bool joins(const TextBuffer &before, const Edit &edit)
{
    const LineNumber first = before.position_of(edit.at).line;
    const Offset end{edit.at.value + edit.removed.size()};
    return newlines_in(edit.removed) == one_line && first.value < before.line_count() &&
           edit.at == before.line_end(first) &&
           blank_only(before.text_range(before.line_start(LineNumber{first.value + 1}), end));
}

[[nodiscard]] bool within(LineNumber line, LineNumber first, LineNumber last) noexcept
{
    return line.value >= first.value && line.value <= last.value;
}
} // namespace

VimMarkPosition vim_mark_position_of(const TextBuffer &text, Offset at)
{
    const LineNumber line = text.position_of(at).line;
    return VimMarkPosition{line, at.value - text.line_start(line).value};
}

std::optional<Offset> vim_mark_offset(const TextBuffer &text, const VimMarkPosition &position)
{
    if (position.line.value < one_line || position.line.value > text.line_count())
    {
        return std::nullopt;
    }
    return vim_line_and_column(text, position.line, position.byte);
}

std::expected<VimMarkPosition, VimRepeatFailure> vim_mark_named(const VimMarks &marks,
                                                                char32_t name)
{
    std::optional<VimMarkPosition> mark;
    if (const auto index = named_index(name))
    {
        mark = marks.named.at(index.value());
    }
    else if (is_context(name))
    {
        mark = marks.previous_context;
    }
    else if (name == U'.')
    {
        mark = marks.last_change;
    }
    else
    {
        return std::unexpected(VimRepeatFailure::refused);
    }
    if (!mark.has_value())
    {
        return std::unexpected(VimRepeatFailure::not_found);
    }
    return mark.value();
}

std::expected<VimMarks, VimRepeatFailure> vim_mark_set(const VimMarks &marks, char32_t name,
                                                       const VimMarkPosition &at)
{
    VimMarks next = marks;
    if (const auto index = named_index(name))
    {
        next.named.at(index.value()) = at;
        return next;
    }
    if (is_context(name))
    {
        // `m'` は動かなくても残す（Vim の setmark_pos が w_prev_pcmark も同じ値にする）。
        next.previous_context = at;
        next.prior_context = std::nullopt;
        return next;
    }
    if (kept_unset(name))
    {
        return next;
    }
    return std::unexpected(VimRepeatFailure::refused);
}

VimMarks vim_marks_jumped(const VimMarks &marks, const VimMarkPosition &from)
{
    VimMarks next = marks;
    next.prior_context = marks.previous_context;
    next.previous_context = from;
    return next;
}

VimMarks vim_marks_checked(const VimMarks &marks, const VimMarkPosition &caret)
{
    VimMarks next = marks;
    if (marks.prior_context.has_value() &&
        (!marks.previous_context.has_value() || marks.previous_context.value() == caret))
    {
        next.previous_context = marks.prior_context;
    }
    next.prior_context = std::nullopt;
    return next;
}

namespace
{
// 行を消す削除の形（決定 6）。行単位の削除（dd・行の先頭から改行まで）、本文の全体、最後の行までの
// 削除（最終行の dd・行の内容の終わりから本文の末尾まで）、行単位の変更（cj・最後の行の内容の
// 終わりまで）。どれでもなければ文字単位の削除で、最後の行の残りが最初の行へつながる。
[[nodiscard]] VimMarkShift removed_shape(const TextBuffer &before, const Edit &edit,
                                         VimMarkShift shift)
{
    const Offset end{edit.at.value + edit.removed.size()};
    const bool from_start = edit.at == before.line_start(shift.first);
    const bool to_size = end.value == before.size_bytes();
    if (from_start && end == before.line_start(shift.last))
    {
        shift.gone_first = shift.first;
        shift.merges = false;
        return shift;
    }
    if (from_start && shift.first.value == one_line && to_size)
    {
        shift.gone_first = shift.first;
        shift.gone_last = shift.last;
        shift.merges = false;
        return shift;
    }
    if ((edit.at == before.line_end(shift.first) && to_size) ||
        (from_start && end == before.line_end(shift.last)))
    {
        shift.gone_last = shift.last;
        shift.merges = false;
        return shift;
    }
    // J gJ（改行と字下げだけの削除）はつないだ位置へ自然に写す。
    shift.joined = joins(before, edit);
    return shift;
}

[[nodiscard]] VimMarkShift shift_of(const TextBuffer &before, const Edit &edit)
{
    const std::size_t removed_lines = newlines_in(edit.removed);
    const LineNumber first = before.position_of(edit.at).line;
    const LineNumber last = line_after(first, removed_lines);
    const Offset end{edit.at.value + edit.removed.size()};
    VimMarkShift shift{first,
                       last,
                       line_after(first, one_line),
                       line_before(last, one_line),
                       removed_lines > 0,
                       false,
                       edit.at.value - before.line_start(first).value,
                       end.value - before.line_start(last).value,
                       edit.inserted.size(),
                       removed_lines,
                       newlines_in(edit.inserted),
                       false};
    if (removed_lines > 0)
    {
        shift = removed_shape(before, edit, shift);
    }
    // 行の先頭へ改行で終わる本文を入れたら（P O・行の置き換え）その行のマークも押し下げる。
    const bool whole_lines = shift.gone_first == first && !shift.merges;
    shift.pushes_first = edit.at == before.line_start(first) && ends_with_newline(edit.inserted) &&
                         (removed_lines == 0 || whole_lines);
    return shift;
}

// 消した行の後ろのマーク。last の残りは first へつなぎ、下の行は消した行数ぶん上げる。
[[nodiscard]] VimMarkPosition lifted(VimMarkPosition at, const VimMarkShift &shift)
{
    if (shift.removed_lines == 0)
    {
        return at;
    }
    if (shift.merges && at.line == shift.last)
    {
        // J gJ は自然な位置、文字単位の削除は Vim の op_delete と同じに桁へ残りの長さを足す。
        const std::size_t kept = at.byte > shift.end_byte ? at.byte - shift.end_byte : 0;
        return VimMarkPosition{shift.first, shift.joined
                                                ? shift.at_byte + shift.inserted_bytes + kept
                                                : shift.at_byte + at.byte};
    }
    if (at.line.value > shift.first.value)
    {
        at.line = line_before(at.line, shift.removed_lines);
    }
    return at;
}

[[nodiscard]] std::optional<VimMarkPosition> shifted(const std::optional<VimMarkPosition> &mark,
                                                     const VimMarkShift &shift)
{
    if (!mark.has_value())
    {
        return std::nullopt;
    }
    if (shift.removed_lines > 0 && within(mark.value().line, shift.gone_first, shift.gone_last))
    {
        return std::nullopt;
    }
    VimMarkPosition at = lifted(mark.value(), shift);
    const bool pushed =
        at.line.value > shift.first.value || (shift.pushes_first && at.line == shift.first);
    if (shift.inserted_lines > 0 && pushed)
    {
        at.line = line_after(at.line, shift.inserted_lines);
    }
    return at;
}
} // namespace

// 挿入した行は、行の先頭へ改行で終わる本文を入れたら（P O）その行から下、ほかは下の行だけを
// 動かす（i<CR> は分けた行のマークを動かさない）。
VimMarks marks_adjusted(const VimMarks &marks, const TextBuffer &before, const Edit &given)
{
    const Edit edit = narrowed(given);
    if (newlines_in(edit.removed) == 0 && newlines_in(edit.inserted) == 0)
    {
        return marks;
    }
    const VimMarkShift shift = shift_of(before, edit);
    VimMarks next = marks;
    for (auto &mark : next.named)
    {
        mark = shifted(mark, shift);
    }
    next.previous_context = shifted(marks.previous_context, shift);
    next.prior_context = shifted(marks.prior_context, shift);
    next.last_change = shifted(marks.last_change, shift);
    return next;
}

VimMarkPosition vim_last_change_of(const TextBuffer &before, const Edit &edit)
{
    const LineNumber first = before.position_of(edit.at).line;
    const bool at_line_end = edit.at == before.line_end(first);
    // 最後の行までを消した（最終行の dd）・行の下へ行を足した（o・最終行の下の p）は、Vim の
    // changed_lines と同じに次の行の行頭（消したなら本文の外で、`'.` は E19 になる）。
    const bool removed_below = at_line_end && starts_with_newline(edit.removed) &&
                               edit.inserted.empty() &&
                               edit.at.value + edit.removed.size() == before.size_bytes();
    const bool appended_below =
        at_line_end && edit.removed.empty() && starts_with_newline(edit.inserted);
    // J gJ は Vim の do_join が消した次の行の行頭（del_lines の changed_lines）。
    if (removed_below || appended_below || joins(before, narrowed(edit)))
    {
        return VimMarkPosition{line_after(first, one_line), 0};
    }
    return VimMarkPosition{first, edit.at.value - before.line_start(first).value};
}
} // namespace nenenib::core
