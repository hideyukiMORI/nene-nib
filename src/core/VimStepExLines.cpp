// Ex の `:d` `:y` と `:g` `:v` の engine（ADR 0168 の決定 1・5・段 3a）。範囲は `:s` と同じ
// ex_lines_resolved の 1 本で行に解き、レジスタは書き手 registers_written の 1 本へ渡し、行を消す
// のは 1 つの効果（VimReplaceBlock・undo 1 単位）。`:g` は範囲の行を 1 回走って当たる行の集合を
// 決め、`:d` はその行を、`:s` の族は置換の本体（global_substituted）を集合の行に 1 回だけ流す。
#include "VimStepDetail.hpp"

#include "ExGlobalRequest.hpp"
#include "ExLineVerb.hpp"
#include "ExLinesRequest.hpp"
#include "LineNumber.hpp"
#include "VimMarkPosition.hpp"
#include "VimPattern.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimRegisterSelection.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
namespace
{
using detail::context_marked;
using detail::ex_landing_column;
using detail::ex_lines_resolved;
using detail::ex_noticed;
using detail::ex_rested;
using detail::register_selection_of;
using detail::registers_written;

// 消す・写す行の塊（原本の行の番号・両端を含む・1 始まり）。
using LineChunk = std::pair<std::size_t, std::size_t>;
// 書き手の後の状態と、OS のクリップボードへ出す本文（`"+` `"*` とクリップボードの無名）。
using WrittenLines = std::pair<VimState, std::optional<VimRegister>>;
// 書き先のレジスタの名前（無ければ無名）と書き方（消すか写すか）。
using LineWrite = std::pair<std::optional<char>, VimOperator>;

// 'report' の既定（Vim の p_report）。これを超える行数のときだけ報せる。
constexpr std::size_t report_lines = 2;
// 数字のレジスタの深さ（`"1`〜`"9`）。これより前の塊は 1 回の書きに畳んでも同じ答えになる。
constexpr std::size_t numbered_depth = 9;

[[nodiscard]] VimSearchNotice notice_of(VimSearchNoticeKind kind, std::string pattern = {})
{
    return VimSearchNotice{kind, std::move(pattern), std::nullopt};
}

[[nodiscard]] VimSearchNotice counted_notice(VimSearchNoticeKind kind, std::size_t count,
                                             std::string pattern = {})
{
    VimSearchNotice notice = notice_of(kind, std::move(pattern));
    notice.count = count;
    return notice;
}

// 行の本文を行単位のレジスタの形（行ごとに LF）で out へ足す。
void lines_appended(std::string &out, const TextBuffer &text, const LineChunk &chunk)
{
    for (std::size_t line = chunk.first; line <= chunk.second; ++line)
    {
        out += text.line_text(LineNumber{line});
        out += '\n';
    }
}

// 書き手へ渡す値（行単位の本文の列・ADR 0168 の段 3a）。Vim の `:g` は塊ごとに `:d`
// を流すので、塊の 数だけ `"1` が繰り下がる。`"9` より前に出る塊は 1
// 回の書きに畳む（名前つきの上書きも追記も、数字の
// レジスタも、畳まずに書いた答えと同じ）ので、値は多くて 10 個。
[[nodiscard]] std::vector<std::string> written_values(const TextBuffer &text,
                                                      std::span<const LineChunk> chunks)
{
    const std::size_t folded = chunks.size() > numbered_depth ? chunks.size() - numbered_depth : 0;
    std::vector<std::string> values;
    values.reserve(chunks.size() - folded + 1);
    if (folded > 0)
    {
        values.emplace_back();
        for (std::size_t index = 0; index < folded; ++index)
        {
            lines_appended(values.back(), text, chunks[index]);
        }
    }
    for (std::size_t index = folded; index < chunks.size(); ++index)
    {
        lines_appended(values.emplace_back(), text, chunks[index]);
    }
    return values;
}

// 値を 1 つ書き手へ渡す（選んだレジスタは Ex の引数のもの）。
[[nodiscard]] std::expected<detail::WrittenRegisters, VimRepeatFailure>
written_once(const VimState &written, const std::optional<VimRegisterSelection> &selection,
             std::string value, VimOperator operation)
{
    VimState writer = written;
    writer.selected_register = selection;
    return registers_written(writer, VimRegister{std::move(value), VimRegisterKind::lines},
                             operation, VimNumberedRule::by_extent);
}

// 塊を順に書き手（registers_written の 1 本）へ渡した後の状態。状態は ex_rested のまま（オペレータ
// 待ちと `.` の直前の変更を保つ）、レジスタの欄だけを書き手の答えにする。
[[nodiscard]] std::expected<WrittenLines, VimRepeatFailure>
written_lines(const VimState &state, const TextBuffer &text, std::span<const LineChunk> chunks,
              LineWrite write)
{
    const auto selection = write.first.has_value()
                               ? register_selection_of(static_cast<char32_t>(write.first.value()))
                               : std::nullopt;
    VimState written = state;
    std::optional<VimRegister> clipboard;
    for (std::string &value : written_values(text, chunks))
    {
        auto result = written_once(written, selection, std::move(value), write.second);
        if (!result)
        {
            return std::unexpected(result.error());
        }
        written = std::move(result.value().first);
        if (result.value().second.has_value())
        {
            clipboard = std::move(result.value().second);
        }
    }
    VimState next = ex_rested(state);
    next.registers = written.registers;
    next.numbered = written.numbered;
    next.small_delete = written.small_delete;
    next.unnamed_register = written.unnamed_register;
    return WrittenLines{std::move(next), std::move(clipboard)};
}

// 続く塊を 1 つの行の塊に畳んだ列（消す範囲）。
[[nodiscard]] std::vector<LineChunk> merged_runs(std::span<const LineChunk> chunks)
{
    std::vector<LineChunk> runs;
    for (const LineChunk &chunk : chunks)
    {
        if (!runs.empty() && runs.back().second + 1 == chunk.first)
        {
            runs.back().second = chunk.second;
            continue;
        }
        runs.push_back(chunk);
    }
    return runs;
}

// 行の塊を消す範囲。最後の行までを消すときは前の行の末尾から（改行を 1 つだけ消す・`dd` と同じ）。
[[nodiscard]] OffsetRange run_range(const TextBuffer &text, const LineChunk &run)
{
    const std::size_t count = text.line_count();
    if (run.second < count)
    {
        return OffsetRange{text.line_start(LineNumber{run.first}),
                           text.line_start(LineNumber{run.second + 1})};
    }
    const Offset begin = run.first > 1 ? text.line_end(LineNumber{run.first - 1}) : Offset{0};
    return OffsetRange{begin, Offset{text.size_bytes()}};
}

// 消した後のキャレット（文書の位置・消した後の本文の上）。最後に消した行の次の行（最後の行までを
// 消したなら残った最後の行）の最初の非空白（Vim の op_delete の beginline(BL_WHITE | BL_FIX)・
// 実測）。全部の行を消したなら 0。
[[nodiscard]] Offset removed_caret(const TextBuffer &text, std::span<const LineChunk> runs,
                                   std::span<const VimBlockEdit> edits)
{
    const LineChunk &last = runs.back();
    const bool through_end = last.second >= text.line_count();
    if (through_end && last.first == 1)
    {
        return Offset{0};
    }
    const LineNumber target{through_end ? last.first - 1 : last.second + 1};
    const std::size_t start = text.line_start(target).value;
    std::size_t removed = 0;
    for (const VimBlockEdit &edit : edits)
    {
        removed +=
            edit.range.end.value <= start ? edit.range.end.value - edit.range.begin.value : 0;
    }
    return Offset{start - removed + ex_landing_column(text.line_text(target))};
}

[[nodiscard]] std::size_t lines_in(std::span<const LineChunk> chunks) noexcept
{
    std::size_t lines = 0;
    for (const LineChunk &chunk : chunks)
    {
        lines += chunk.second - chunk.first + 1;
    }
    return lines;
}

// 行の塊を消した 1 歩（Vim の op_delete の行単位・`:g` は塊ごとに `:d`）。空の本文は消すものが無い
// （Vim の ML_EMPTY・レジスタも変えない）。undo の戻り先は最初に消した行の最初の非空白（Vim の
// ex_operators の beginline(BL_SOL | BL_FIX) の後の u_save・実測）、変更の印は最初に消した行の頭
// （del_lines の changed_lines）。
[[nodiscard]] VimStep removed_lines(const VimState &state, const VimEditorView &view,
                                    std::span<const LineChunk> chunks, std::optional<char> name)
{
    const TextBuffer &text = view.text;
    if (text.size_bytes() == 0)
    {
        return VimStep{ex_rested(state), VimNoEffect{}};
    }
    auto written = written_lines(state, text, chunks, LineWrite{name, VimOperator::remove});
    if (!written)
    {
        return ex_noticed(state, notice_of(VimSearchNoticeKind::not_supported), written.error());
    }
    const std::vector<LineChunk> runs = merged_runs(chunks);
    std::vector<VimBlockEdit> edits;
    edits.reserve(runs.size());
    for (const LineChunk &run : runs)
    {
        edits.push_back(VimBlockEdit{run_range(text, run), std::string{}});
    }
    const Offset caret = removed_caret(text, runs, edits);
    const std::size_t lines = lines_in(runs);
    const LineNumber first{runs.front().first};
    const Offset head = text.line_start(first);
    // `'.` は最後の塊の `:d` が changed_lines に渡す位置（前の塊が消した行の分だけ上がった行の
    // 桁 0・Vim は塊ごとに del_lines を呼び最後の塊が残る・実測 #1654）。
    const std::size_t above = lines_in(chunks.first(chunks.size() - 1));
    VimStep step{std::move(written.value().first),
                 VimReplaceBlock{std::move(edits), caret,
                                 VimMarkPosition{LineNumber{chunks.back().first - above}, 0}}};
    step.clipboard = std::move(written.value().second);
    step.restore = Offset{head.value + ex_landing_column(text.line_text(first))};
    step.change = VimChangeMark{VimChange::changed, text.position_of(head)};
    if (lines == text.line_count())
    {
        step.notice = notice_of(VimSearchNoticeKind::no_lines_in_buffer);
    }
    else if (lines > report_lines)
    {
        step.notice = counted_notice(VimSearchNoticeKind::fewer_lines, lines);
    }
    return context_marked(std::move(step), view);
}

// 行を写した 1 歩（Vim の op_yank の行単位）。キャレットは動かさず、`''` も置かない。
[[nodiscard]] VimStep yanked_lines(const VimState &state, const VimEditorView &view,
                                   const LineChunk &chunk, std::optional<char> name)
{
    auto written =
        written_lines(state, view.text, std::span{&chunk, 1}, LineWrite{name, VimOperator::yank});
    if (!written)
    {
        return ex_noticed(state, notice_of(VimSearchNoticeKind::not_supported), written.error());
    }
    VimStep step{std::move(written.value().first), VimNoEffect{}};
    step.clipboard = std::move(written.value().second);
    const std::size_t lines = chunk.second - chunk.first + 1;
    // `"_` は Vim の op_yank が書く前に返るので報せない。
    if (lines > report_lines && name != std::optional<char>{'_'})
    {
        step.notice =
            counted_notice(VimSearchNoticeKind::lines_yanked, lines,
                           name.has_value() ? std::string(1, name.value()) : std::string{});
    }
    return step;
}

// 回数（`:d 3`・`:2,3d 2`）の行の塊（Vim の do_one_cmd: 範囲の最後の行から回数の行・本文の終わり
// まで）。回数 0 は値なし（E939）。
[[nodiscard]] std::optional<LineChunk>
counted_chunk(detail::ExLineSpan span, std::optional<std::size_t> count, std::size_t total) noexcept
{
    if (!count.has_value())
    {
        return span;
    }
    if (count.value() == 0)
    {
        return std::nullopt;
    }
    const std::size_t room = total - span.second;
    return LineChunk{span.second, span.second + std::min(count.value() - 1, room)};
}

// `:g` の行を範囲の中で 1 回走り、当たる（inverted なら当たらない）行を上から集める（Vim の
// ex_global の pass 1）。
[[nodiscard]] std::vector<std::size_t> marked_lines(const TextBuffer &text,
                                                    const VimPattern &pattern,
                                                    detail::ExLineSpan span, bool inverted)
{
    std::vector<std::size_t> marked;
    for (std::size_t line = span.first; line <= span.second; ++line)
    {
        const bool found = pattern.matched(text.line_text(LineNumber{line}), 0).has_value();
        if (found != inverted)
        {
            marked.push_back(line);
        }
    }
    return marked;
}

// `:g/{pat}/d [x] [count]` の塊。選んだ行から回数の行を消し、前の塊が消した行は飛ばす（Vim は消した
// 行の印も消す）。
[[nodiscard]] std::vector<LineChunk> global_chunks(std::span<const std::size_t> marked,
                                                   std::size_t count, std::size_t total)
{
    std::vector<LineChunk> chunks;
    for (const std::size_t line : marked)
    {
        if (!chunks.empty() && line <= chunks.back().second)
        {
            continue;
        }
        chunks.emplace_back(line, line + std::min(count - 1, total - line));
    }
    return chunks;
}

// `:g` が選んだ行に命令を流す 1 歩（Vim の global_exe）。
[[nodiscard]] VimStep global_run(const VimState &state, const VimEditorView &view,
                                 const ExGlobalCommand &command,
                                 std::span<const std::size_t> marked)
{
    if (const auto *const substitute = std::get_if<ExSubstituteRequest>(&command))
    {
        return detail::global_substituted(state, view, *substitute, marked);
    }
    const auto *const removal = std::get_if<ExLinesRequest>(&command);
    if (removal == nullptr)
    {
        std::unreachable();
    }
    const ExLinesRequest &lines = *removal;
    if (lines.count == std::optional<std::size_t>{0})
    {
        return ex_noticed(state, notice_of(VimSearchNoticeKind::positive_count),
                          VimRepeatFailure::refused);
    }
    const std::vector<LineChunk> chunks =
        global_chunks(marked, lines.count.value_or(1), view.text.line_count());
    return removed_lines(state, view, chunks, lines.register_name);
}

// `:g` のパターンの前向きの形（空なら直前の検索・無ければ E35）。
[[nodiscard]] std::expected<std::string, VimSearchNoticeKind>
global_pattern(const VimState &state, const ExGlobalRequest &request)
{
    if (!request.pattern.empty())
    {
        return detail::ex_forward_pattern(request.pattern, request.delimiter);
    }
    auto last = detail::ex_last_search(state);
    if (!last.has_value())
    {
        return std::unexpected(VimSearchNoticeKind::no_previous_pattern);
    }
    return std::move(last).value();
}
} // namespace

VimStep vim_lines_edited(const VimState &state, const VimEditorView &view,
                         const ExLinesRequest &request)
{
    const auto span = ex_lines_resolved(view, request.range);
    if (!span)
    {
        return ex_noticed(state, notice_of(span.error()), VimRepeatFailure::refused);
    }
    const auto chunk = counted_chunk(span.value(), request.count, view.text.line_count());
    if (!chunk.has_value())
    {
        return ex_noticed(state, notice_of(VimSearchNoticeKind::positive_count),
                          VimRepeatFailure::refused);
    }
    switch (request.verb)
    {
    case ExLineVerb::delete_lines:
        return removed_lines(state, view, std::span{&chunk.value(), 1}, request.register_name);
    case ExLineVerb::yank_lines:
        return yanked_lines(state, view, chunk.value(), request.register_name);
    }
    std::unreachable();
}

VimStep vim_global(const VimState &state, const VimEditorView &view, const ExGlobalRequest &request)
{
    const auto span = request.range.has_value()
                          ? ex_lines_resolved(view, request.range)
                          : std::expected<detail::ExLineSpan, VimSearchNoticeKind>{
                                detail::ExLineSpan{1, view.text.line_count()}};
    if (!span)
    {
        return ex_noticed(state, notice_of(span.error()), VimRepeatFailure::refused);
    }
    const auto pattern_text = global_pattern(state, request);
    if (!pattern_text)
    {
        return ex_noticed(state, notice_of(pattern_text.error()), VimRepeatFailure::refused);
    }
    const auto pattern =
        VimPattern::parse(pattern_text.value(), VimSearchDirection::forward, view.search_case);
    if (!pattern)
    {
        return ex_noticed(state,
                          VimSearchNotice{VimSearchNoticeKind::unsupported_pattern, std::string{},
                                          pattern.error()},
                          VimRepeatFailure::refused);
    }
    VimState next = detail::ex_pattern_remembered(ex_rested(state), view, pattern_text.value());
    const std::vector<std::size_t> marked =
        marked_lines(view.text, pattern.value(), span.value(), request.inverted);
    if (marked.empty())
    {
        VimStep step{std::move(next), VimNoEffect{}};
        step.notice = notice_of(request.inverted ? VimSearchNoticeKind::global_every_line
                                                 : VimSearchNoticeKind::global_not_found,
                                request.pattern.empty() ? pattern_text.value() : request.pattern);
        return step;
    }
    return global_run(next, view, request.command, marked);
}
} // namespace nenenib::core
