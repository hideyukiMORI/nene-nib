// `:s` の族の engine（ADR 0168 の決定 1〜5・段 1）。Ex の `:s` `:&` `:~` と NORMAL の `&` `g&` は
// どれも vim_substituted の 1 本で、範囲を行に解き、前の置換と前のパターンを引き、置換の本体
// （SubstituteAll の substituted・通常モードの「すべて置換」と同じ 1 本）で行ごとの編集を作る。
#include "VimStepDetail.hpp"

#include "ExAddress.hpp"
#include "ExRange.hpp"
#include "ExSubstituteRequest.hpp"
#include "LineEnding.hpp"
#include "SubstituteAll.hpp"
#include "SubstituteConfirmation.hpp"
#include "VimCaret.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimColumnWish.hpp"
#include "VimMarkEdits.hpp"
#include "VimSearchSnapshot.hpp"
#include "VimSmartcase.hpp"
#include "VimSubstituteMemory.hpp"
#include "VimWantedColumn.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
using detail::context_marked;

// 範囲を解いた最初と最後の行（両端を含む・1 始まり）。
using LineSpan = std::pair<std::size_t, std::size_t>;
// 解く途中の最初と最後の行（本文の外と負の値も持つ）。
using RawSpan = std::pair<std::int64_t, std::int64_t>;

// 番地の足し算の飽和の端（ExRange と同じ）。
constexpr std::int64_t line_limit = std::int64_t{1} << 62;

// 置換の後の NORMAL。`.` の直前の変更は保つ（`:s` は `.` の記録を変えない・`x:s/a/X/<CR>j.` は
// `x` を繰り返す・実測）。
// オペレータ待ちの `:s`（`d:s/a/b/`）は保留のオペレータと回数・レジスタ・`.` の組み立てをそのまま
// 残し、controller が置換の前のキャレットから今のキャレットまでで閉じる（ADR 0168 の追補 16）。
[[nodiscard]] VimState rested(const VimState &state)
{
    if (state.pending.has_value())
    {
        return state;
    }
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.last_change = state.last_change;
    return next;
}

// 報せと失敗だけの 1 歩。状態は鍵を食べ終えた NORMAL。
[[nodiscard]] VimStep noticed(const VimState &state, VimSearchNotice notice,
                              VimRepeatFailure failure)
{
    VimStep step{rested(state), VimNoEffect{}};
    step.notice = std::move(notice);
    step.failure = failure;
    return step;
}

[[nodiscard]] VimSearchNotice notice_of(VimSearchNoticeKind kind, std::string pattern = {})
{
    return VimSearchNotice{kind, std::move(pattern), std::nullopt};
}

[[nodiscard]] std::optional<std::size_t> line_of(const std::optional<VimMarkPosition> &position)
{
    return position.has_value() ? std::optional{position.value().line.value} : std::nullopt;
}

// `'<` `'>`（前の選択の上の行と下の行）。
[[nodiscard]] std::optional<std::size_t> visual_line(const VimMarks &marks, char name)
{
    if (!marks.last_visual.has_value())
    {
        return std::nullopt;
    }
    const auto &visual = marks.last_visual.value();
    return name == '<' ? std::min(visual.anchor.line.value, visual.caret.line.value)
                       : std::max(visual.anchor.line.value, visual.caret.line.value);
}

// 記号のマークの行（`''` `` '` `` `'.` `'^`）。
[[nodiscard]] std::optional<std::size_t> symbol_line(const VimMarks &marks, char name)
{
    if (name == '\'' || name == '`')
    {
        return line_of(marks.previous_context);
    }
    if (name == '.')
    {
        return line_of(marks.last_change);
    }
    return name == '^' ? line_of(marks.last_insert) : std::nullopt;
}

// マークの行（`'a`〜`'z` `'<` `'>` `''` `` '` `` `'.` `'^`）。無ければ値なし（E20）。
[[nodiscard]] std::optional<std::size_t> mark_line(const VimMarks &marks, char name)
{
    if (name >= 'a' && name <= 'z')
    {
        return line_of(marks.named.at(static_cast<std::size_t>(name - 'a')));
    }
    return name == '<' || name == '>' ? visual_line(marks, name) : symbol_line(marks, name);
}

[[nodiscard]] std::int64_t clamped_line(std::size_t line) noexcept
{
    return static_cast<std::int64_t>(
        std::min<std::size_t>(line, static_cast<std::size_t>(line_limit)));
}

// 番地 1 つの基点の行（Vim の get_address）。cursor は基点を省いた番地と `.` の行。
[[nodiscard]] std::expected<std::int64_t, VimSearchNoticeKind>
address_base(const VimEditorView &view, const ExAddress &address, std::size_t cursor)
{
    switch (address.base)
    {
    case ExAddressBase::current:
        return clamped_line(cursor);
    case ExAddressBase::last:
        return clamped_line(view.text.line_count());
    case ExAddressBase::number:
        return clamped_line(address.number);
    case ExAddressBase::mark:
        break;
    }
    const auto line = mark_line(*view.marks, address.mark);
    if (!line.has_value())
    {
        return std::unexpected(VimSearchNoticeKind::mark_not_set);
    }
    return clamped_line(line.value());
}

// 番地の列を後ろの 2 つの行に（Vim の parse_cmd_range）。`;` の後ろの番地は前の番地の行から
// 数える。負の行は E16。1 つなら同じ行。
[[nodiscard]] std::expected<RawSpan, VimSearchNoticeKind>
addressed_lines(const VimEditorView &view, const ExRange &range, std::size_t caret)
{
    RawSpan span{clamped_line(caret), clamped_line(caret)};
    std::size_t cursor = caret;
    for (const ExAddress &address : range.addresses)
    {
        cursor = address.after_semicolon ? static_cast<std::size_t>(span.second) : cursor;
        const auto base = address_base(view, address, cursor);
        if (!base || base.value() + address.offset < 0)
        {
            return std::unexpected(base ? VimSearchNoticeKind::invalid_range : base.error());
        }
        span = RawSpan{span.second, base.value() + address.offset};
    }
    return range.addresses.size() == 1 ? RawSpan{span.second, span.second} : span;
}

// 範囲を行に解く（Vim の do_one_cmd の後半）。逆向きは E493（Vim の :normal の中の答え）、本文の
// 外は E16、0 行目は 1 行目（correct_range）。範囲が無ければキャレットの行。
[[nodiscard]] std::expected<LineSpan, VimSearchNoticeKind>
resolved_lines(const VimEditorView &view, const std::optional<ExRange> &range)
{
    const std::size_t caret = view.text.position_of(view.selection.caret).line.value;
    if (!range.has_value())
    {
        return LineSpan{caret, caret};
    }
    const auto span = addressed_lines(view, range.value(), caret);
    if (!span)
    {
        return std::unexpected(span.error());
    }
    const auto [first, last] = span.value();
    if (first > last)
    {
        return std::unexpected(VimSearchNoticeKind::backwards_range);
    }
    if (static_cast<std::uint64_t>(last) > view.text.line_count())
    {
        return std::unexpected(VimSearchNoticeKind::invalid_range);
    }
    return LineSpan{std::max<std::size_t>(static_cast<std::size_t>(first), 1),
                    std::max<std::size_t>(static_cast<std::size_t>(last), 1)};
}

// `[...]` の中か（区切りを逃がさない所）。
[[nodiscard]] bool in_collection(bool inside, char letter) noexcept
{
    if (letter == '[')
    {
        return true;
    }
    return letter == ']' ? false : inside;
}

// 区切り separator を `[...]` の外で `\` で逃がしたパターン（照合器は区切りを offset として断る・
// `\/` は Vim でも `/` の文字）。`:s#a/b#` の `/` と、`?` の検索の向きで覚える直前の検索に使う。
[[nodiscard]] std::string separated(std::string_view pattern, char separator)
{
    std::string escaped;
    escaped.reserve(pattern.size());
    bool collection = false;
    for (std::size_t at = 0; at < pattern.size(); ++at)
    {
        const char letter = pattern[at];
        if (letter == '\\' && at + 1 < pattern.size())
        {
            escaped += pattern.substr(at, 2);
            ++at;
            continue;
        }
        collection = in_collection(collection, letter);
        if (letter == separator && !collection)
        {
            escaped += '\\';
        }
        escaped += letter;
    }
    return escaped;
}

// 直前の検索のパターンを前向きの区切りで読める形に（`?` の検索の `\?` は `?` の文字・`/`
// は逃がす）。
// `?` を区切りにしたパターンの `\?` を `?` の文字にした前向きの形（Vim の skip_regexp_ex の
// `dirc == '?'`・`:s?a\?b?X?` は `a?b` に当たる・実測）。`/` は逃がす。
[[nodiscard]] std::string question_unescaped(std::string_view text)
{
    std::string plain;
    plain.reserve(text.size());
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        const std::size_t length = text[at] == '\\' && at + 1 < text.size() ? 2 : 1;
        const std::string_view piece = text.substr(at, length);
        plain += piece == "\\?" ? std::string_view{"?"} : piece;
        at += length - 1;
    }
    return separated(plain, '/');
}

[[nodiscard]] std::string forward_text(const VimSearchSnapshot &search)
{
    const std::string_view text = search.text();
    return search.direction() == VimSearchDirection::forward ? std::string(text)
                                                             : question_unescaped(text);
}

// 大小のフラグ（`i` は ignore・`I` は sensitive）。
[[nodiscard]] VimSubstituteFlags cased(VimSubstituteFlags flags, char letter) noexcept
{
    if (letter == 'i' || letter == 'I')
    {
        flags.letter_case = letter == 'i' ? VimPatternCase::ignore : VimPatternCase::sensitive;
    }
    return flags;
}

// フラグの文字 1 つを当てる（Vim の ex_substitute の while）。`r` は前の置換のパターンの代わりに
// 最後に使ったパターンを読む印で、last_used に立てる。
[[nodiscard]] VimSubstituteFlags flag_applied(VimSubstituteFlags flags, char letter,
                                              bool &last_used) noexcept
{
    if (letter == 'g')
    {
        flags.every = !flags.every;
    }
    else if (letter == 'n')
    {
        flags.count = true;
    }
    else if (letter == 'e')
    {
        flags.error = !flags.error;
    }
    else if (letter == 'c')
    {
        flags.confirm = !flags.confirm;
    }
    last_used = last_used || letter == 'r';
    return cased(flags, letter);
}

// UTF-8（改行は '\n'）を文書の改行の形にしたときのバイト数。
[[nodiscard]] std::size_t document_bytes(std::string_view utf8, LineEnding ending)
{
    const auto newlines = static_cast<std::size_t>(std::ranges::count(utf8, '\n'));
    return utf8.size() + newlines * (newline_of(ending).size() - 1);
}

// 置換の後のキャレットの行の中の桁（Vim の beginline(BL_WHITE | BL_FIX) と、`$` の後の繰り返しの
// coladvance(MAXCOL)）。最初の非空白、空白だけなら最後の文字、空の行なら 0。
[[nodiscard]] std::size_t landing_column(std::string_view line, bool end_column)
{
    if (line.empty())
    {
        return 0;
    }
    if (end_column)
    {
        return vim_character_start(line, Offset{line.size()}).value;
    }
    std::size_t column = 0;
    while (column + 1 < line.size() && (line[column] == ' ' || line[column] == '\t'))
    {
        ++column;
    }
    return column;
}

// 数えるだけの後のキャレット（Vim の do_sub は w_cursor を old_cursor へ戻し、`$` の後の繰り返し
// （endcolumn）なら coladvance(MAXCOL) で行末、ほかは beginline(BL_WHITE | BL_FIX)）。
[[nodiscard]] Offset counted_caret(const TextBuffer &text, Offset caret, bool end_column)
{
    if (!end_column)
    {
        return vim_first_non_blank(text, caret);
    }
    const LineNumber line = text.position_of(caret).line;
    return Offset{text.line_start(line).value + landing_column(text.line_text(line), true)};
}

// 最後の編集より前の編集が増やしたバイト数（文書の改行の形）。負なら減らした。
[[nodiscard]] std::int64_t earlier_growth(const SubstituteResult &result, LineEnding ending)
{
    std::int64_t growth = 0;
    for (std::size_t index = 0; index + 1 < result.edits.size(); ++index)
    {
        const VimBlockEdit &edit = result.edits.at(index);
        growth += static_cast<std::int64_t>(document_bytes(edit.utf8, ending)) -
                  static_cast<std::int64_t>(edit.range.end.value - edit.range.begin.value);
    }
    return growth;
}

// 置換の後のキャレット（文書の位置・置換の後の本文の上）。最後に当たった行の、最後の置き換えが
// 入れた改行の後ろ（改行が無ければその行）の着地（決定 5・Vim の do_sub の cursor.lnum）。
[[nodiscard]] Offset substituted_caret(const TextBuffer &text, const SubstituteResult &result,
                                       bool end_column)
{
    const LineEnding ending = text.line_ending();
    const VimBlockEdit &last = result.edits.back();
    const std::string content = text.line_text(result.last_line);
    const std::size_t start = text.line_start(result.last_line).value;
    const auto cut = last.utf8.rfind('\n');
    const bool broken = cut != std::string::npos;
    // 着地の行の頭（置換の前の本文の上の位置で、前の編集の増減を後で足す）と、行の中身。
    const std::size_t head =
        broken ? last.range.begin.value +
                     document_bytes(std::string_view{last.utf8}.substr(0, cut + 1), ending)
               : start;
    std::string line = broken ? last.utf8.substr(cut + 1)
                              : content.substr(0, last.range.begin.value - start) + last.utf8;
    line += std::string_view{content}.substr(last.range.end.value - start);
    const std::int64_t growth = earlier_growth(result, ending);
    return Offset{static_cast<std::size_t>(static_cast<std::int64_t>(head) + growth) +
                  landing_column(line, end_column)};
}

// 前の置換を覚えた状態（Vim の old_sub・reg_prev_sub・subflags・RE_SUBST と RE_LAST）。直前の
// 検索（`n` が探す）も置換のパターンにし、向きは前の検索のまま（無ければ前向き）・`:nohlsearch` で
// 止めた強調を戻す（Vim の save_re_pat）。
[[nodiscard]] VimState remembered(VimState next, const VimEditorView &view,
                                  VimSubstituteMemory memory, std::string_view pattern)
{
    const VimSearchDirection direction = next.last_search.has_value()
                                             ? next.last_search.value().direction()
                                             : VimSearchDirection::forward;
    next.last_search = VimSearchSnapshot::from(
        separated(pattern, direction == VimSearchDirection::forward ? '/' : '?'), direction,
        view.search_case, VimSmartcase::honored);
    if (next.highlight == VimSearchHighlight::suspended)
    {
        next.highlight = VimSearchHighlight::on;
    }
    next.last_substitute = std::make_shared<const VimSubstituteMemory>(std::move(memory));
    return next;
}

// 当たりが無い・大きすぎるときの 1 歩（決定 5）。`e` なら E486 を出さず失敗にもしない。
[[nodiscard]] VimStep unsubstituted(VimState next, SubstituteFailure failure,
                                    const VimSubstituteFlags &flags, std::string_view pattern)
{
    switch (failure)
    {
    case SubstituteFailure::no_match:
        if (!flags.error)
        {
            return VimStep{std::move(next), VimNoEffect{}};
        }
        return noticed(next,
                       notice_of(VimSearchNoticeKind::pattern_not_found, std::string(pattern)),
                       VimRepeatFailure::not_found);
    case SubstituteFailure::too_large:
        return detail::failed(VimStep{std::move(next), VimNoEffect{}}, VimRepeatFailure::too_large);
    // 群を引けない当たりは本文を 1 バイトも変えずに断る（追補 13・群を黙って空にしない）。
    case SubstituteFailure::too_complex:
        return noticed(next, notice_of(VimSearchNoticeKind::pattern_too_complex),
                       VimRepeatFailure::refused);
    }
    std::unreachable();
}

// 置換の数の報せ。置き換えは 'report'（既定 2）を超えたときだけ（Vim の do_sub_msg）。
[[nodiscard]] std::optional<VimSearchNotice> counted_notice(const SubstituteResult &result,
                                                            bool counted)
{
    constexpr std::size_t report = 2;
    if (!counted && result.substitutions <= report)
    {
        return std::nullopt;
    }
    VimSearchNotice notice =
        notice_of(counted ? VimSearchNoticeKind::matches : VimSearchNoticeKind::substitutions);
    notice.count = result.substitutions;
    notice.lines = result.lines;
    return notice;
}

// 置き換えた 1 歩（決定 5）。undo の戻り先は最初に置き換えた行の頭（Vim の u_savesub の
// uh_cursor・実測）、キャレットは最後に置き換えた行の最初の非空白、`''` は置換の前のキャレット。
[[nodiscard]] VimStep substituted_step(VimState next, const VimEditorView &view,
                                       SubstituteResult result, bool end_column)
{
    const bool counted = result.edits.empty();
    auto notice = counted_notice(result, counted);
    // 数えるだけ（`n`）は今の行に留まる（範囲がほかの行でも行は動かない・Vim の do_sub の末尾は
    // old_cursor へ戻してから置き換えた道と同じ着地・#1607 の 1 つ目と #1622 の M1 の実測）。
    if (counted)
    {
        next.wanted_column =
            end_column
                ? std::optional{VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{0}}}
                : std::nullopt;
        VimStep step{std::move(next),
                     VimMoveTo{counted_caret(view.text, view.selection.caret, end_column)}};
        step.notice = std::move(notice);
        return context_marked(std::move(step), view);
    }
    if (end_column)
    {
        next.wanted_column = VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{0}};
    }
    const Offset caret = substituted_caret(view.text, result, end_column);
    const Offset restore =
        view.text.line_start(view.text.position_of(result.edits.front().range.begin).line);
    VimStep step{std::move(next), VimReplaceBlock{std::move(result.edits), caret}};
    step.notice = std::move(notice);
    step.restore = restore;
    // Vim の do_sub は changed_lines(first_line, 0, …) を呼ぶ（最初に置き換えた行の桁 0・ADR 0181
    // の 追補 #1374 の位置）。
    step.change = VimChangeMark{VimChange::changed, view.text.position_of(restore)};
    return context_marked(std::move(step), view);
}

// 置換のパターンの出どころ（Vim の which_pat）。typed で空でなければ打ったパターン、typed の空と
// `:~` と `r` は最後に使ったパターン（直前の検索）、`:s` `:&` `&` は前の置換のパターン。
[[nodiscard]] std::optional<std::string> pattern_of(const VimState &state,
                                                    const ExSubstituteRequest &request,
                                                    const VimSubstituteMemory &memory,
                                                    bool last_used)
{
    if (request.source == ExSubstituteSource::typed && !request.pattern.empty())
    {
        return request.delimiter == '?' ? question_unescaped(request.pattern)
                                        : separated(request.pattern, '/');
    }
    if (request.source != ExSubstituteSource::repeated || last_used)
    {
        return state.last_search.has_value()
                   ? std::optional{forward_text(state.last_search.value())}
                   : std::nullopt;
    }
    return memory.pattern;
}

// 前の広げた置き換え（`~` が入れる・無ければ空）。
[[nodiscard]] std::string previous_of(const VimState &state)
{
    return state.last_substitute != nullptr ? state.last_substitute->previous : std::string{};
}

// パターンを探す前に断る理由（パターンが無ければ E35・`e` の無い回数 0 は E939・Vim の
// ex_substitute の順）。断らなければ値なし。
[[nodiscard]] std::optional<VimSearchNoticeKind>
early_refusal(const VimSubstituteMemory &memory, bool has_pattern,
              const ExSubstituteRequest &request) noexcept
{
    if (request.count == std::optional<std::size_t>{0} && memory.flags.error)
    {
        return VimSearchNoticeKind::positive_count;
    }
    return has_pattern ? std::nullopt : std::optional{VimSearchNoticeKind::no_previous_pattern};
}

// 回数（`:s/a/b/ 3`）は範囲の最後の行から回数の行（本文の終わりまで・Vim の ex_substitute）。回数 0
// と、最後の行 + 回数 - 1 が INT_MAX を越える形（Vim の 32 ビットの行番号が溢れて範囲が空になる・
// `:3s/a/X/g 2147483646` は E486・実測）は空の範囲で、値なし（ADR 0168 の追補 14）。
[[nodiscard]] std::optional<LineSpan>
counted_lines(LineSpan span, const ExSubstituteRequest &request, const TextBuffer &text) noexcept
{
    if (!request.count.has_value())
    {
        return span;
    }
    constexpr std::size_t line_number_limit = 2147483647;
    const std::size_t count = request.count.value();
    if (count == 0 || span.second > line_number_limit - (count - 1))
    {
        return std::nullopt;
    }
    const std::size_t room = text.line_count() - span.second;
    return LineSpan{span.second, span.second + std::min(count - 1, room)};
}

// 置換の覚えを新しくした値と、パターン（無ければ E35）。前の置換が要る形で無ければ E33。
// typed は打った置き換えを、ほかは前の置き換えを使い、`~` を前の広げた置き換えで解く。
[[nodiscard]] std::expected<std::pair<VimSubstituteMemory, std::optional<std::string>>,
                            VimSearchNoticeKind>
renewed_memory(const VimState &state, const ExSubstituteRequest &request)
{
    const VimSubstituteMemory *const before = state.last_substitute.get();
    if (request.source != ExSubstituteSource::typed && before == nullptr)
    {
        return std::unexpected(VimSearchNoticeKind::no_previous_substitute);
    }
    VimSubstituteMemory memory = before != nullptr ? *before : VimSubstituteMemory{};
    memory.replacement =
        request.source == ExSubstituteSource::typed ? request.replacement : memory.replacement;
    bool last_used = request.source == ExSubstituteSource::tilde;
    VimSubstituteFlags flags = request.keep_flags ? memory.flags : VimSubstituteFlags{};
    for (const char letter : request.flags)
    {
        flags = flag_applied(flags, letter, last_used);
    }
    // `n` は問わずに数えるだけ（Vim の do_count は do_ask を倒し、倒した値を `&&` が保つ）。
    flags.confirm = flags.confirm && !flags.count;
    memory.flags = flags;
    // 置換のパターン（RE_SUBST）と広げた置き換え（reg_prev_sub）は、E939 と E35 の後では変えない
    // （Vim は回数の検査と search_regcomp の後でだけ書く・`:s/a/X/ 0` の後の `:&` と `n` は前の
    // パターン・実測）。書くのは vim_substituted の remembered の手前。
    auto pattern = pattern_of(state, request, memory, last_used);
    return std::pair{std::move(memory), std::move(pattern)};
}

// `$` の後の前の置換の繰り返しか（着地を行末にする・Vim の endcolumn・実測）。
[[nodiscard]] bool repeats_at_line_end(const VimState &state,
                                       const ExSubstituteRequest &request) noexcept
{
    return request.source != ExSubstituteSource::typed && state.wanted_column.has_value() &&
           state.wanted_column.value().wish == VimColumnWish::at_line_end;
}

// `c` の問いの最初の値の種（問いの文の置き換えと `a` の着地・ADR 0168 の決定 6）。
[[nodiscard]] SubstituteConfirmation confirm_seed(std::string shown, bool end_column)
{
    SubstituteConfirmation seed;
    seed.shown = std::move(shown);
    seed.end_column = end_column;
    return seed;
}
} // namespace

VimStep vim_substituted(const VimState &state, const VimEditorView &view,
                        const ExSubstituteRequest &request)
{
    const auto span = resolved_lines(view, request.range);
    if (!span)
    {
        return noticed(state, notice_of(span.error()), VimRepeatFailure::refused);
    }
    auto renewed = renewed_memory(state, request);
    if (!renewed)
    {
        return noticed(state, notice_of(renewed.error()), VimRepeatFailure::refused);
    }
    auto [memory, pattern] = std::move(renewed).value();
    VimState next = rested(state);
    const auto refused = early_refusal(memory, pattern.has_value(), request);
    if (refused.has_value())
    {
        next.last_substitute = std::make_shared<const VimSubstituteMemory>(std::move(memory));
        return noticed(next, notice_of(refused.value()), VimRepeatFailure::refused);
    }
    // early_refusal がパターンの無い形を断ったので、ここでは値がある。
    const std::string pattern_text = pattern.value_or(std::string{});
    memory.pattern = pattern_text;
    memory.previous = vim_tilde_expanded(memory.replacement, previous_of(state));
    const VimSubstituteFlags flags = memory.flags;
    const auto replacement = SubstituteReplacement::vim(memory.previous);
    // 問いの文の置き換え（Vim の do_sub の sub は regtilde の後）。
    std::string shown = memory.previous;
    next = remembered(std::move(next), view, std::move(memory), pattern_text);
    const auto parsed = VimPattern::parse(pattern_text, VimSearchDirection::forward,
                                          flags.letter_case.value_or(view.search_case));
    if (!parsed)
    {
        return noticed(next,
                       VimSearchNotice{VimSearchNoticeKind::unsupported_pattern, std::string{},
                                       parsed.error()},
                       VimRepeatFailure::refused);
    }
    // 空の範囲（`e` の回数 0・行番号の溢れ）は当たりが無いのと同じ（`e` なら黙って続く・実測）。
    const auto lines = counted_lines(span.value(), request, view.text);
    if (!lines.has_value())
    {
        return unsubstituted(std::move(next), SubstituteFailure::no_match, flags, pattern_text);
    }
    const auto [first, last] = lines.value();
    const bool end_column = repeats_at_line_end(state, request);
    const SubstituteRequest scope{LineNumber{first}, LineNumber{last},    flags.every,
                                  flags.count,       view.document_limit, view.capture_limit};
    // `c` は当たりの列を元の本文から 1 回で計画して最初の当たりを問う（決定 6・段 2a）。
    if (flags.confirm)
    {
        auto matches = substitute_matches(view.text, parsed.value(), replacement, scope);
        return matches ? detail::confirm_started(std::move(next), view, std::move(matches).value(),
                                                 confirm_seed(std::move(shown), end_column))
                       : unsubstituted(std::move(next), matches.error(), flags, pattern_text);
    }
    auto result = substituted(view.text, parsed.value(), replacement, scope);
    return result ? substituted_step(std::move(next), view, std::move(result).value(), end_column)
                  : unsubstituted(std::move(next), result.error(), flags, pattern_text);
}

namespace detail
{
Offset substitute_landing(const TextBuffer &text, const SubstituteResult &result, bool end_column)
{
    return substituted_caret(text, result, end_column);
}

std::optional<VimSearchNotice> substitute_notice(const SubstituteResult &result)
{
    return counted_notice(result, false);
}

VimStep substitute_action(const VimState &state, const VimEditorView &view, VimAction action)
{
    if (action == VimAction::repeat_substitute_all)
    {
        // `g&` は `:%s//~/&`（Vim の nv_g_cmd）。
        ExRange all{{ExAddress{ExAddressBase::number, 1, '\0', 0, false},
                     ExAddress{ExAddressBase::last, 0, '\0', 0, false}}};
        return vim_substituted(state, view,
                               ExSubstituteRequest{std::move(all), ExSubstituteSource::typed, '/',
                                                   std::string{}, std::string{"~"}, true,
                                                   std::string{}, std::nullopt});
    }
    // `&` は `:s`、回数は `:.,.+(N-1)s`（Vim の nv_optrans が回数と `:s` を流す）。
    std::optional<ExRange> range;
    if (state.count.has_value())
    {
        const auto below = clamped_line(resolved_count(state) - 1);
        range = ExRange{{ExAddress{ExAddressBase::current, 0, '\0', 0, false},
                         ExAddress{ExAddressBase::current, 0, '\0', below, false}}};
    }
    return vim_substituted(state, view,
                           ExSubstituteRequest{std::move(range), ExSubstituteSource::repeated, '/',
                                               std::string{}, std::string{}, false, std::string{},
                                               std::nullopt});
}
} // namespace detail
} // namespace nenenib::core
