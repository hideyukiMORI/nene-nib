#include "VimBlockProbe.hpp"

#include "Milestone.hpp"
#include "Offset.hpp"
#include "ProbeWorkload.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "TextObjectProbe.hpp"
#include "TextObjectWorkload.hpp"
#include "VimBlockEdit.hpp"
#include "VimBlockRange.hpp"
#include "VimBlockWidth.hpp"
#include "VimCharacter.hpp"
#include "VimEditorView.hpp"
#include "VimInsertBlock.hpp"
#include "VimInsertString.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimMoveTo.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimRemoveRange.hpp"
#include "VimReplaceBlock.hpp"
#include "VimReplay.hpp"
#include "VimSelect.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimVisualReselect.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests::performance
{
namespace
{
// 16 MiB の 1 行。8 バイトの ASCII の並びを繰り返す（#1198 の切り分けと同じ ASCII の 1 行）。
constexpr std::size_t giant_bytes = std::size_t{16} * 1024U * 1024U;
constexpr std::string_view giant_unit = "abcdefg ";

[[nodiscard]] std::string giant_line()
{
    std::string text;
    text.reserve(giant_bytes);
    while (text.size() < giant_bytes)
    {
        text += giant_unit;
    }
    return text;
}

[[nodiscard]] core::VimStep stepped(const core::VimState &state, const core::TextBuffer &text,
                                    const core::Selection &selection, core::VimKey key)
{
    return core::vim_step(state, core::VimEditorView{text, selection, std::nullopt}, key);
}

// 矩形の `r` `p` と `.` の 3 本が角を置く所（本文の真ん中・`a` の上）。
[[nodiscard]] core::Offset giant_middle() noexcept
{
    return core::Offset{giant_bytes / 2U};
}

[[nodiscard]] std::expected<core::TextBuffer, ProbeFailure> indexed_giant(VimBlockWorkload workload,
                                                                          const std::string &input)
{
    const auto parsed = core::TextBuffer::from_utf8(input);
    if (!parsed.has_value() || input != vim_block_input(workload) ||
        parsed.value().line_count() != 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::cerr << "preparedPieces=" << parsed.value().piece_count()
              << " preparedHash=" << checksum_of(input) << '\n';
    return parsed.value().with_virtual_columns();
}

[[nodiscard]] core::TextBuffer with_block_edits(const core::TextBuffer &text,
                                                std::vector<core::VimBlockEdit> edits)
{
    std::ranges::sort(edits, [](const core::VimBlockEdit &one, const core::VimBlockEdit &other)
                      { return other.range.begin < one.range.begin; });
    core::TextBuffer next = text;
    for (const core::VimBlockEdit &edit : edits)
    {
        next = next.replaced(edit.range.begin, edit.range.end, edit.utf8);
    }
    return next;
}

// 効果を本文と選択へ写す小さな替え玉（矩形の鍵と INSERT の文字だけ・単体試験の block_applied と
// 同じ形）。
[[nodiscard]] std::pair<core::TextBuffer, core::Selection> applied(const core::TextBuffer &text,
                                                                   const core::Selection &selection,
                                                                   const core::VimEffect &effect)
{
    if (const auto *moved = std::get_if<core::VimMoveTo>(&effect))
    {
        return {text, core::collapsed_at(moved->caret)};
    }
    if (const auto *selected = std::get_if<core::VimSelect>(&effect))
    {
        return {text, selected->selection};
    }
    if (const auto *typed = std::get_if<core::VimInsertString>(&effect))
    {
        return {text.insert(selection.caret, typed->utf8),
                core::collapsed_at(core::Offset{selection.caret.value + typed->utf8.size()})};
    }
    if (const auto *inserted = std::get_if<core::VimInsertBlock>(&effect))
    {
        return {with_block_edits(text, inserted->edits), core::collapsed_at(inserted->caret)};
    }
    return {text, selection};
}

// 鍵を 1 つ流して効果を写す。本文・選択・状態を進める。
void pressed(core::VimState &state, core::TextBuffer &text, core::Selection &selection,
             core::VimKey key)
{
    const core::VimStep step = stepped(state, text, selection, key);
    auto [next_text, next_selection] = applied(text, selection, step.effect);
    text = std::move(next_text);
    selection = next_selection;
    state = step.next;
}
} // namespace

std::string vim_block_input(VimBlockWorkload workload)
{
    switch (workload)
    {
    case VimBlockWorkload::insert_giant_1line:
    case VimBlockWorkload::replace_giant_1line:
    case VimBlockWorkload::put_giant_1line:
    case VimBlockWorkload::repeat_giant_1line:
    case VimBlockWorkload::insert_ctrl_w_giant_1line:
    case VimBlockWorkload::count_h_giant_1line:
    case VimBlockWorkload::count_l_giant_1line:
        return giant_line();
    case VimBlockWorkload::sentence_motion_giant_1line:
        return text_object_input(TextObjectWorkload::sentence_giant);
    }
    std::unreachable();
}

// `$` は区間の外で流す。区間は `<C-v>` の鍵と、その後に frame が引く矩形（vim_block_range_for）と、
// `I` の鍵。`I` は `$` の矩形の左の桁（行の最後の文字）の前に入るので、効果はキャレットの移動だけ。
std::expected<std::uint64_t, ProbeFailure> vim_block_inserted(VimBlockWorkload workload,
                                                              const std::string &input,
                                                              application::TimingPort &timing)
{
    const auto parsed = core::TextBuffer::from_utf8(input);
    if (!parsed.has_value() || input != vim_block_input(workload) ||
        parsed.value().line_count() != 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const core::TextBuffer text = parsed.value().with_virtual_columns();
    const core::VimState resting = core::vim_resting_state(
        core::VimRegister{std::string{}, core::VimRegisterKind::characters});
    const core::VimStep end =
        stepped(resting, text, core::collapsed_at(core::Offset{0}), core::VimCharacter{U'$'});
    const auto *moved = std::get_if<core::VimMoveTo>(&end.effect);
    if (moved == nullptr)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const core::Selection caret = core::collapsed_at(moved->caret);
    std::cerr << "preparedPieces=" << text.piece_count() << " preparedHash=" << checksum_of(input)
              << '\n';
    timing.mark(core::Milestone::probe_started);
    const core::VimStep block = stepped(end.next, text, caret, core::VimSpecialKey::control_v);
    const core::VimBlockRange drawn =
        core::vim_block_range_for(text, caret, block.next.wanted_column);
    const core::VimStep inserted = stepped(block.next, text, caret, core::VimCharacter{U'I'});
    timing.mark(core::Milestone::probe_finished);
    if (block.next.mode != core::VimMode::visual_block ||
        inserted.next.mode != core::VimMode::insert || !inserted.next.block_insert.has_value() ||
        drawn.lines.size() != 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto &insert = inserted.next.block_insert.value();
    if (insert.at_byte != input.size() - 1U || insert.line_bytes != input.size() ||
        insert.column.value != input.size())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) + insert.at_byte + insert.line_bytes + insert.column.value +
           drawn.width.columns;
}
// 真ん中の 1 文字の矩形の `r`（#1237）。区間は `<C-v>` と `r` と置く文字の 3 鍵。効果は 1 行の
// 置き換えで、本文は置く文字の 1 つ。
std::expected<std::uint64_t, ProbeFailure> vim_block_replaced(VimBlockWorkload workload,
                                                              const std::string &input,
                                                              application::TimingPort &timing)
{
    const auto prepared = indexed_giant(workload, input);
    if (!prepared.has_value())
    {
        return std::unexpected(prepared.error());
    }
    const core::TextBuffer &text = prepared.value();
    const core::Selection caret = core::collapsed_at(giant_middle());
    const core::VimState resting = core::vim_resting_state(
        core::VimRegister{std::string{}, core::VimRegisterKind::characters});
    timing.mark(core::Milestone::probe_started);
    const core::VimStep block = stepped(resting, text, caret, core::VimSpecialKey::control_v);
    const core::VimStep pending = stepped(block.next, text, caret, core::VimCharacter{U'r'});
    const core::VimStep replaced = stepped(pending.next, text, caret, core::VimCharacter{U'Z'});
    timing.mark(core::Milestone::probe_finished);
    const auto *effect = std::get_if<core::VimReplaceBlock>(&replaced.effect);
    if (effect == nullptr || effect->edits.size() != 1U || effect->edits.front().utf8 != "Z" ||
        effect->edits.front().range.begin != giant_middle() || effect->caret != giant_middle())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) + effect->edits.front().range.end.value + effect->caret.value;
}

// 真ん中の文字の後ろへ 1 行・幅 2 の矩形のレジスタを貼る `p`（#1237）。区間は `p` の 1 鍵。
std::expected<std::uint64_t, ProbeFailure>
vim_block_put(VimBlockWorkload workload, const std::string &input, application::TimingPort &timing)
{
    const auto prepared = indexed_giant(workload, input);
    if (!prepared.has_value())
    {
        return std::unexpected(prepared.error());
    }
    const core::TextBuffer &text = prepared.value();
    const core::Selection caret = core::collapsed_at(giant_middle());
    const core::VimState resting = core::vim_resting_state(
        core::VimRegister{"xy", core::VimRegisterKind::block, core::VimBlockWidth{2}});
    timing.mark(core::Milestone::probe_started);
    const core::VimStep put = stepped(resting, text, caret, core::VimCharacter{U'p'});
    timing.mark(core::Milestone::probe_finished);
    const auto *effect = std::get_if<core::VimInsertBlock>(&put.effect);
    const core::Offset after{giant_middle().value + 1U};
    if (effect == nullptr || effect->edits.size() != 1U || effect->edits.front().utf8 != "xy" ||
        effect->edits.front().range.begin != after || effect->edits.front().range.end != after ||
        effect->caret != after)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) + effect->caret.value + effect->edits.front().utf8.size();
}

// 真ん中の 1 文字の矩形で `Ix<Esc>` を打った後の `.`（#1237）。打つところは区間の外で、区間は
// `.` の鍵と、controller と同じく選び直してから流す再生の鍵（`I` `x` `<Esc>`）。
std::expected<std::uint64_t, ProbeFailure> vim_block_repeated(VimBlockWorkload workload,
                                                              const std::string &input,
                                                              application::TimingPort &timing)
{
    const auto prepared = indexed_giant(workload, input);
    if (!prepared.has_value())
    {
        return std::unexpected(prepared.error());
    }
    core::TextBuffer text = prepared.value();
    core::Selection selection = core::collapsed_at(giant_middle());
    core::VimState state = core::vim_resting_state(
        core::VimRegister{std::string{}, core::VimRegisterKind::characters});
    for (const core::VimKey &key :
         {core::VimKey{core::VimSpecialKey::control_v}, core::VimKey{core::VimCharacter{U'I'}},
          core::VimKey{core::VimCharacter{U'x'}}, core::VimKey{core::VimSpecialKey::escape}})
    {
        pressed(state, text, selection, key);
    }
    if (selection.caret != giant_middle() || text.size_bytes() != input.size() + 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    timing.mark(core::Milestone::probe_started);
    const core::VimStep repeat = stepped(state, text, selection, core::VimCharacter{U'.'});
    const auto *replay = std::get_if<core::VimReplay>(&repeat.effect);
    if (replay == nullptr || !replay->reselect.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::vector<core::VimKey> keys = replay->keys;
    state = repeat.next;
    selection = core::vim_visual_reselect(text, selection.caret, replay->reselect.value());
    for (const core::VimKey &key : keys)
    {
        pressed(state, text, selection, key);
    }
    timing.mark(core::Milestone::probe_finished);
    if (state.mode != core::VimMode::normal || selection.caret != giant_middle() ||
        text.size_bytes() != input.size() + 2U ||
        text.text_range(giant_middle(), core::Offset{giant_middle().value + 3U}) != "xxa")
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) + text.size_bytes() + keys.size();
}

// 真ん中の `a` の前で `i` を打ってからの INSERT の `<C-w>`（#1258）。`i` は区間の外で、区間は
// `<C-w>` の 1 鍵。始めの桁がキャレットと同じなので消せる左端は行頭で、消すのは手前の空白と語の
// 8 バイト。
std::expected<std::uint64_t, ProbeFailure> vim_insert_word_cut(VimBlockWorkload workload,
                                                               const std::string &input,
                                                               application::TimingPort &timing)
{
    const auto prepared = indexed_giant(workload, input);
    if (!prepared.has_value())
    {
        return std::unexpected(prepared.error());
    }
    core::TextBuffer text = prepared.value();
    core::Selection selection = core::collapsed_at(giant_middle());
    core::VimState state = core::vim_resting_state(
        core::VimRegister{std::string{}, core::VimRegisterKind::characters});
    pressed(state, text, selection, core::VimCharacter{U'i'});
    if (state.mode != core::VimMode::insert || selection.caret != giant_middle())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    timing.mark(core::Milestone::probe_started);
    const core::VimStep cut = stepped(state, text, selection, core::VimSpecialKey::control_w);
    timing.mark(core::Milestone::probe_finished);
    const auto *effect = std::get_if<core::VimRemoveRange>(&cut.effect);
    const core::Offset word{giant_middle().value - giant_unit.size()};
    if (effect == nullptr || effect->range.begin != word || effect->range.end != giant_middle())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) + effect->range.begin.value + effect->range.end.value;
}

// 真ん中から `99999999h`（#1258）。回数の数字は区間の外で、区間は `h` の 1 鍵。回数が行頭までの
// 文字の数より多いので行頭に着く。
std::expected<std::uint64_t, ProbeFailure> vim_counted_left(VimBlockWorkload workload,
                                                            const std::string &input,
                                                            application::TimingPort &timing)
{
    const auto prepared = indexed_giant(workload, input);
    if (!prepared.has_value())
    {
        return std::unexpected(prepared.error());
    }
    core::TextBuffer text = prepared.value();
    core::Selection selection = core::collapsed_at(giant_middle());
    core::VimState state = core::vim_resting_state(
        core::VimRegister{std::string{}, core::VimRegisterKind::characters});
    constexpr std::size_t digits = 8;
    for (std::size_t digit = 0; digit < digits; ++digit)
    {
        pressed(state, text, selection, core::VimCharacter{U'9'});
    }
    timing.mark(core::Milestone::probe_started);
    const core::VimStep left = stepped(state, text, selection, core::VimCharacter{U'h'});
    timing.mark(core::Milestone::probe_finished);
    const auto *moved = std::get_if<core::VimMoveTo>(&left.effect);
    if (moved == nullptr || moved->caret != core::Offset{0} || left.failure.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) + giant_middle().value + moved->caret.value;
}

// 真ん中から `99999999l`（#1269）。回数の数字は区間の外で、区間は `l` の 1 鍵。回数が行末までの
// 文字の数より多いので、NORMAL の行の最後の文字（末尾の空白）に着く。
std::expected<std::uint64_t, ProbeFailure> vim_counted_right(VimBlockWorkload workload,
                                                             const std::string &input,
                                                             application::TimingPort &timing)
{
    const auto prepared = indexed_giant(workload, input);
    if (!prepared.has_value())
    {
        return std::unexpected(prepared.error());
    }
    core::TextBuffer text = prepared.value();
    core::Selection selection = core::collapsed_at(giant_middle());
    core::VimState state = core::vim_resting_state(
        core::VimRegister{std::string{}, core::VimRegisterKind::characters});
    constexpr std::size_t digits = 8;
    for (std::size_t digit = 0; digit < digits; ++digit)
    {
        pressed(state, text, selection, core::VimCharacter{U'9'});
    }
    timing.mark(core::Milestone::probe_started);
    const core::VimStep right = stepped(state, text, selection, core::VimCharacter{U'l'});
    timing.mark(core::Milestone::probe_finished);
    const auto *moved = std::get_if<core::VimMoveTo>(&right.effect);
    const core::Offset last{text.size_bytes() - 1U};
    if (moved == nullptr || moved->caret != last || right.failure.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) + giant_middle().value + moved->caret.value;
}
// `)` の 1 鍵（#1316）。文の終わりの無い巨大な 1 行の真ん中から、文の終わりを探す歩きは本文の
// 終わりで尽き（回数 1 の最後なので失敗しない）、NORMAL の寄せで最後の文字へ着く（Vim 9.1 の
// findsent と adjust_cursor）。歩きは #751 の塊の走査で行の終わりまで飛ぶ。
std::expected<std::uint64_t, ProbeFailure> vim_sentence_moved(VimBlockWorkload workload,
                                                              const std::string &input,
                                                              application::TimingPort &timing)
{
    const auto prepared = indexed_giant(workload, input);
    if (!prepared.has_value())
    {
        return std::unexpected(prepared.error());
    }
    const core::TextBuffer &text = prepared.value();
    const core::Offset middle{text.size_bytes() / 2U};
    const core::VimState state = core::vim_resting_state(
        core::VimRegister{std::string{}, core::VimRegisterKind::characters});
    timing.mark(core::Milestone::probe_started);
    const core::VimStep moved_step =
        stepped(state, text, core::collapsed_at(middle), core::VimCharacter{U')'});
    timing.mark(core::Milestone::probe_finished);
    const auto *moved = std::get_if<core::VimMoveTo>(&moved_step.effect);
    const core::Offset last{text.size_bytes() - 1U};
    if (moved == nullptr || moved->caret != last || moved_step.failure.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) + middle.value + moved->caret.value;
}
} // namespace nenenib::tests::performance
