#include "SubstituteAll.hpp"

#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "SubstituteScan.hpp"
#include "SubstituteTally.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimPatternCaptures.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
// 当たりの後に次を探す位置。数えなかった長さ 0 の当たり（skipped）は 1 文字（Vim の 1 文字・
// mb_ptr2len）進める。行の終わりでは動かない。
[[nodiscard]] std::size_t searched_from(std::string_view line, VimPatternMatch match,
                                        bool skipped) noexcept
{
    return skipped ? vim_character_end(line, Offset{match.end}).value : match.end;
}

// 行の当たりを Vim の do_sub の順で visit(当たり) へ渡す（決定 5）。前の当たりの終わりにぴったり
// 続く長さ 0 の当たりは数えず 1 文字進めて探し直す。行の終わりに着くか、every でなく 1 つ置いたら
// 止める。
template <typename Visit>
void each_line_match(std::string_view line, const VimPattern &pattern, bool every, Visit &visit)
{
    std::size_t from = 0;
    std::optional<std::size_t> previous;
    for (auto found = pattern.matched(line, 0); found.has_value();
         found = pattern.matched(line, from))
    {
        const VimPatternMatch match = found.value();
        const bool skipped = previous == from && match.end == from;
        if (!skipped)
        {
            visit(match);
            previous = match.end;
        }
        from = searched_from(line, match, skipped);
        if (!every || from >= line.size())
        {
            return;
        }
    }
}

// 当たりだけの群の範囲（`&` `\0`）。
[[nodiscard]] VimPatternCaptures match_only(VimPatternMatch match)
{
    VimPatternCaptures captures{};
    captures.at(0) = match;
    return captures;
}

// 当たりの群の範囲（置き換えが `\1`〜`\9` を読むときだけ引く）。
// 升の数の上限を超える当たりは値なし（追補 13）。
[[nodiscard]] std::optional<VimPatternCaptures>
captures_of(const SubstituteScan &scan, std::string_view line, VimPatternMatch match)
{
    if (scan.groups)
    {
        return scan.pattern.captured(line, match, scan.capture_limit);
    }
    return match_only(match);
}

// 1 周目の 1 つの当たりが足すバイト（文書の改行の形で数える・群を引けなければ印を立てる）。
[[nodiscard]] std::size_t added_bytes(const SubstituteScan &scan, std::string_view line,
                                      VimPatternMatch match, SubstituteTally &tally)
{
    const auto captures = captures_of(scan, line, match);
    tally.too_complex = tally.too_complex || !captures.has_value();
    return scan.replacement.expanded_bytes(line, captures.value_or(match_only(match)),
                                           newline_of(scan.text.line_ending()).size());
}

// 1 周目の 1 行: 数えるだけ。置き換えの本文は作らない。
void line_tallied(const SubstituteScan &scan, std::size_t line, bool count_only,
                  SubstituteTally &tally)
{
    const std::string content = scan.text.line_text(LineNumber{line});
    const std::size_t before = tally.substitutions;
    auto visit = [&](VimPatternMatch match)
    {
        ++tally.substitutions;
        tally.removed += match.end - match.begin;
        tally.added += count_only ? 0 : added_bytes(scan, content, match, tally);
    };
    each_line_match(content, scan.pattern, scan.every, visit);
    if (tally.substitutions == before)
    {
        return;
    }
    tally.first = tally.lines == 0 ? line : tally.first;
    tally.last = line;
    ++tally.lines;
}

// 2 周目の 1 行: 行の最初の当たりから最後の当たりまでを置き換えた本文。当たりが無ければ値なし。
[[nodiscard]] std::optional<VimBlockEdit> line_edited(const SubstituteScan &scan, std::size_t line)
{
    const std::string content = scan.text.line_text(LineNumber{line});
    std::string body;
    std::optional<std::size_t> begin;
    std::size_t copied = 0;
    auto visit = [&](VimPatternMatch match)
    {
        copied = begin.has_value() ? copied : match.begin;
        begin = begin.value_or(match.begin);
        body.append(content, copied, match.begin - copied);
        scan.replacement.appended(body, content,
                                  captures_of(scan, content, match).value_or(match_only(match)));
        copied = match.end;
    };
    each_line_match(content, scan.pattern, scan.every, visit);
    if (!begin.has_value())
    {
        return std::nullopt;
    }
    const Offset start = scan.text.line_start(LineNumber{line});
    return VimBlockEdit{
        OffsetRange{Offset{start.value + begin.value()}, Offset{start.value + copied}},
        std::move(body)};
}

// 2 周目: 当たった行の編集を上の行から emit へ渡す。
template <typename Emit>
void each_line_edit(const SubstituteScan &scan, const SubstituteTally &tally, Emit &&emit)
{
    for (std::size_t line = tally.first; line <= tally.last; ++line)
    {
        auto edit = line_edited(scan, line);
        if (edit.has_value())
        {
            emit(std::move(edit).value());
        }
    }
}

// 当たりの外の本文 kept と、足すバイト added が limit に収まるか。
[[nodiscard]] bool fits(std::size_t kept, std::size_t added, std::size_t limit) noexcept
{
    return kept <= limit && added <= limit - kept;
}

[[nodiscard]] std::expected<SubstituteTally, SubstituteFailure>
checked_tally(const SubstituteScan &scan, const SubstituteRequest &request)
{
    SubstituteTally tally;
    for (std::size_t line = request.first.value; line <= request.last.value; ++line)
    {
        line_tallied(scan, line, request.count_only, tally);
    }
    if (tally.substitutions == 0)
    {
        return std::unexpected(SubstituteFailure::no_match);
    }
    if (tally.too_complex)
    {
        return std::unexpected(SubstituteFailure::too_complex);
    }
    if (!request.count_only &&
        !fits(scan.text.size_bytes() - tally.removed, tally.added, request.limit))
    {
        return std::unexpected(SubstituteFailure::too_large);
    }
    return tally;
}
} // namespace

std::expected<SubstituteResult, SubstituteFailure>
substituted(const TextBuffer &text, const VimPattern &pattern,
            const SubstituteReplacement &replacement, const SubstituteRequest &request)
{
    const SubstituteScan scan{text,
                              pattern,
                              replacement,
                              request.every,
                              replacement.reads_subgroups() && pattern.group_count() > 0,
                              request.capture_limit};
    const auto tally = checked_tally(scan, request);
    if (!tally)
    {
        return std::unexpected(tally.error());
    }
    SubstituteResult result{
        {}, tally.value().substitutions, tally.value().lines, LineNumber{tally.value().last}, 0};
    if (request.count_only)
    {
        return result;
    }
    each_line_edit(scan, tally.value(),
                   [&result](VimBlockEdit edit)
                   {
                       result.breaks +=
                           static_cast<std::size_t>(std::ranges::count(edit.utf8, '\n'));
                       result.edits.push_back(std::move(edit));
                   });
    return result;
}

std::expected<Edit, SubstituteFailure> substituted_all(const TextBuffer &text,
                                                       const VimPattern &pattern,
                                                       std::string_view replacement,
                                                       std::size_t limit)
{
    const SubstituteReplacement literal = SubstituteReplacement::literal(replacement);
    const SubstituteScan scan{text, pattern, literal, true, false, vim_capture_cell_limit};
    const auto tally = checked_tally(
        scan, SubstituteRequest{LineNumber{1}, LineNumber{text.line_count()}, true, false, limit});
    if (!tally)
    {
        return std::unexpected(tally.error());
    }
    std::optional<Offset> at;
    Offset copied{};
    std::string inserted;
    each_line_edit(scan, tally.value(),
                   [&](const VimBlockEdit &edit)
                   {
                       copied = at.has_value() ? copied : edit.range.begin;
                       at = at.value_or(edit.range.begin);
                       inserted += text.text_range(copied, edit.range.begin);
                       inserted += edit.utf8;
                       copied = edit.range.end;
                   });
    const Offset begin = at.value_or(Offset{});
    return Edit{begin, text.text_range(begin, copied), std::move(inserted), begin};
}
} // namespace nenenib::core
