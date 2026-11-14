// `:s///c` の問い（ADR 0168 の決定 6・段 2a）。vim_substituted が元の本文から計画した当たりの列を
// 問いの値（SubstituteConfirmation）に入れ、答えのたびに vim_substitute_answered が置く当たり
// （いまの本文の座標）とキャレットと次の値を返す。問いの状態を持つのは application の入力行で、
// engine は純関数のまま。答えの後のキャレット・`''`・報せ・undo の戻り先は Vim 9.1 の do_sub の
// 実測（D:/NeNeNib/scripts/20261113/e3-1652・`--not-a-term` に `:normal!` で答えを流した 45 形）。
#include "VimStepDetail.hpp"

#include "LineEnding.hpp"
#include "SubstituteResult.hpp"
#include "VimBlockEdit.hpp"
#include "VimConfirmSubstitute.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
using Matches = std::vector<SubstituteMatch>;

// UTF-8（改行は '\n'）を文書の改行の形にしたときのバイト数。
[[nodiscard]] std::size_t document_bytes(std::string_view utf8, LineEnding ending)
{
    const auto newlines = static_cast<std::size_t>(std::ranges::count(utf8, '\n'));
    return utf8.size() + newlines * (newline_of(ending).size() - 1);
}

[[nodiscard]] Offset shifted(Offset at, std::int64_t shift) noexcept
{
    return Offset{static_cast<std::size_t>(static_cast<std::int64_t>(at.value) + shift)};
}

// 当たりのいまの本文の範囲（前に置いた当たりの増減 shift を足す）。
[[nodiscard]] OffsetRange current_range(const SubstituteMatch &match, std::int64_t shift) noexcept
{
    return OffsetRange{shifted(match.range.begin, shift), shifted(match.range.end, shift)};
}

// 当たりを置くと本文が増えるバイト数（負なら減る）。
[[nodiscard]] std::int64_t growth_of(const SubstituteMatch &match, LineEnding ending)
{
    return static_cast<std::int64_t>(document_bytes(match.utf8, ending)) -
           static_cast<std::int64_t>(match.range.end.value - match.range.begin.value);
}

// undo の戻り先（Vim の uh_cursor）。最初に置いた行の答えのたびに、答えた後のカーソル（置けば行の
// 頭・置かなければ当たりの始まり）を覚え、行が終わったときの値が残る（Vim は行を書くときに
// u_savesub を呼ぶ）。置き換えが改行を入れるなら最初に置いた瞬間の行の頭（u_inssub）。桁は元の行の
// 長さで切る（undo で戻った行の中・行末の 1 つ先は NORMAL の寄せが戻す）。
[[nodiscard]] SubstituteConfirmation restore_noted(SubstituteConfirmation asked,
                                                   const TextBuffer &text,
                                                   const SubstituteMatch &match, bool accepted)
{
    const bool first = !asked.first_line.has_value() && accepted;
    if (first)
    {
        asked.first_line = match.line;
        asked.first_start = text.line_start(match.line);
        asked.first_length = text.line_end(match.line).value - asked.first_start.value;
    }
    if (asked.first_line != std::optional{match.line} || (asked.breaks && !first))
    {
        return asked;
    }
    const std::size_t column =
        accepted ? 0 : shifted(match.range.begin, asked.shift).value - asked.first_start.value;
    asked.restore = Offset{asked.first_start.value + std::min(column, asked.first_length)};
    return asked;
}

// 置いた当たりを数える（報せの数と行の数）。
[[nodiscard]] SubstituteConfirmation counted(SubstituteConfirmation asked,
                                             const SubstituteMatch &match, LineEnding ending)
{
    ++asked.substitutions;
    if (asked.counted_line != std::optional{match.line})
    {
        ++asked.lines;
        asked.counted_line = match.line;
    }
    asked.shift += growth_of(match, ending);
    return asked;
}

// `y` `l` の後のキャレット（Vim の do_sub は置いた後に桁を 0 にし、置き換えの改行ごとに行を
// 1 つ進める）: 置いた本文の終わりの行の頭（置いた後の本文の位置）。
[[nodiscard]] Offset accepted_caret(const TextBuffer &text, OffsetRange at, std::string_view utf8)
{
    const auto cut = utf8.rfind('\n');
    if (cut == std::string_view::npos)
    {
        return text.line_start(text.line_containing(at.begin));
    }
    return Offset{at.begin.value + document_bytes(utf8.substr(0, cut + 1), text.line_ending())};
}

// 問いを終える 1 歩。置いた数が 3 つ以上なら報せ（Vim の do_sub_msg）、置いた分があれば戻り先。
[[nodiscard]] VimStep finished(VimState state, const SubstituteConfirmation &asked,
                               std::vector<VimBlockEdit> edits, Offset caret)
{
    const auto restore =
        asked.first_line.has_value() ? std::optional{asked.restore} : std::optional<Offset>{};
    VimStep step{std::move(state),
                 VimConfirmSubstitute{std::move(edits), caret, std::nullopt, restore}};
    step.notice = detail::substitute_notice(
        SubstituteResult{{}, asked.substitutions, asked.lines, LineNumber{1}, 0});
    return step;
}

// 次の当たりを問う 1 歩か、当たりが尽きたら終える 1 歩（キャレットは done）。
[[nodiscard]] VimStep asked_next(VimState state, SubstituteConfirmation asked,
                                 std::vector<VimBlockEdit> edits, Offset done)
{
    const Matches &matches = *asked.matches;
    if (asked.next >= matches.size())
    {
        return finished(std::move(state), asked, std::move(edits), done);
    }
    const Offset caret = shifted(matches.at(asked.next).range.begin, asked.shift);
    return VimStep{std::move(state),
                   VimConfirmSubstitute{std::move(edits), caret, std::move(asked), std::nullopt}};
}

// 置いた当たりの Vim の changed_* の位置（Vim の do_sub は行を書くたびに changed_bytes(lnum, 0)）。
[[nodiscard]] VimStep change_marked_at(VimStep step, const TextBuffer &text, Offset at)
{
    step.change = VimChangeMark{VimChange::changed,
                                text.position_of(text.line_start(text.line_containing(at)))};
    return step;
}

// `y`（continues）と `l`: いまの当たりを置く。
[[nodiscard]] VimStep accepted(VimState state, const VimEditorView &view,
                               const SubstituteConfirmation &asking, bool continues)
{
    const SubstituteMatch &match = asking.matches->at(asking.next);
    const OffsetRange at = current_range(match, asking.shift);
    auto asked =
        counted(restore_noted(asking, view.text, match, true), match, view.text.line_ending());
    ++asked.next;
    const Offset caret = accepted_caret(view.text, at, match.utf8);
    std::vector<VimBlockEdit> edits{VimBlockEdit{at, match.utf8}};
    auto step = continues ? asked_next(std::move(state), std::move(asked), std::move(edits), caret)
                          : finished(std::move(state), asked, std::move(edits), caret);
    return change_marked_at(std::move(step), view.text, at.begin);
}

// `n`（continues）と `q`: いまの当たりを置かない。キャレットは問うていた当たり（Vim の do_sub は
// 問う前に当たりの始まりへ置く）。
[[nodiscard]] VimStep declined(VimState state, const VimEditorView &view,
                               const SubstituteConfirmation &asking, bool continues)
{
    const SubstituteMatch &match = asking.matches->at(asking.next);
    const Offset at = shifted(match.range.begin, asking.shift);
    auto asked = restore_noted(asking, view.text, match, false);
    ++asked.next;
    return continues ? asked_next(std::move(state), std::move(asked), {}, at)
                     : finished(std::move(state), asked, {}, at);
}

// `a` の残りの当たりを行ごとの編集に畳む（行の最初の当たりから最後の当たりまで・間の本文は写す・
// substituted と同じ形）。asked は数えた値になる。
[[nodiscard]] std::vector<VimBlockEdit> rest_edits(const TextBuffer &text,
                                                   SubstituteConfirmation &asked)
{
    // 編集はどれも置く前の本文の座標なので、最初の当たりの shift で作る。
    const std::int64_t base = asked.shift;
    std::vector<VimBlockEdit> edits;
    std::optional<LineNumber> line;
    for (; asked.next < asked.matches->size(); ++asked.next)
    {
        const SubstituteMatch &match = asked.matches->at(asked.next);
        const OffsetRange at = current_range(match, base);
        if (line == std::optional{match.line})
        {
            edits.back().utf8 += text.text_range(edits.back().range.end, at.begin);
            edits.back().utf8 += match.utf8;
            edits.back().range.end = at.end;
        }
        else
        {
            edits.push_back(VimBlockEdit{at, match.utf8});
        }
        line = match.line;
        asked =
            counted(restore_noted(std::move(asked), text, match, true), match, text.line_ending());
    }
    return edits;
}

// `a`: いまの当たりから後ろを全部置いて終える。着地は `c` の無い `:s` と同じ（最後に置き換えた行の
// 最初の非空白・`$` の後の繰り返しは行末・Vim は `a` で do_ask を倒すので beginline を通る）。
[[nodiscard]] VimStep accepted_all(VimState state, const VimEditorView &view,
                                   SubstituteConfirmation asked)
{
    auto edits = rest_edits(view.text, asked);
    std::size_t breaks = 0;
    for (const VimBlockEdit &edit : edits)
    {
        breaks += static_cast<std::size_t>(std::ranges::count(edit.utf8, '\n'));
    }
    const SubstituteResult landing{edits, asked.substitutions, asked.lines,
                                   view.text.line_containing(edits.back().range.begin), breaks};
    const Offset caret = detail::substitute_landing(view.text, landing, asked.end_column);
    if (asked.end_column)
    {
        state.wanted_column = VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{0}};
    }
    const Offset first = edits.front().range.begin;
    return change_marked_at(finished(std::move(state), asked, std::move(edits), caret), view.text,
                            first);
}
} // namespace

namespace detail
{
VimStep confirm_started(VimState next, const VimEditorView &view, Matches matches,
                        SubstituteConfirmation seed)
{
    // 再生（`.`・`@`）の中では問わない（Vim は後ろの鍵を答えとして読む・段 2a の限界）。
    if (vim_key_replayed(view.source))
    {
        VimStep step{std::move(next), VimNoEffect{}};
        step.notice = VimSearchNotice{VimSearchNoticeKind::confirm_in_replay, {}, std::nullopt};
        return failed(std::move(step), VimRepeatFailure::refused);
    }
    seed.breaks = matches.front().utf8.find('\n') != std::string::npos;
    const Offset caret = matches.front().range.begin;
    seed.matches = std::make_shared<const Matches>(std::move(matches));
    VimStep step{std::move(next), VimConfirmSubstitute{{}, caret, std::move(seed), std::nullopt}};
    // Vim の do_sub は最初の当たりで setpcmark する（答えが全部 `n` でも `''` は置く）。
    return context_marked(std::move(step), view);
}
} // namespace detail

VimStep vim_substitute_answered(const VimState &state, const VimEditorView &view,
                                const SubstituteAnswer &answer)
{
    const SubstituteConfirmation &asking = answer.confirmation;
    // 答えの後の欲しい列はキャレットから決め直す（Vim の do_sub の後は curswant を置き直す）。
    VimState next = state;
    next.wanted_column = std::nullopt;
    switch (answer.reply)
    {
    case SubstituteReply::yes:
        return accepted(std::move(next), view, asking, true);
    case SubstituteReply::last:
        return accepted(std::move(next), view, asking, false);
    case SubstituteReply::no:
        return declined(std::move(next), view, asking, true);
    case SubstituteReply::quit:
        return declined(std::move(next), view, asking, false);
    case SubstituteReply::all:
        return accepted_all(std::move(next), view, asking);
    }
    std::unreachable();
}
} // namespace nenenib::core
