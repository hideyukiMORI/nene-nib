#include "VimBlockProbe.hpp"

#include "Milestone.hpp"
#include "Offset.hpp"
#include "ProbeWorkload.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimBlockRange.hpp"
#include "VimCharacter.hpp"
#include "VimEditorView.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimMoveTo.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"

#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

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
} // namespace

std::string vim_block_input(VimBlockWorkload workload)
{
    switch (workload)
    {
    case VimBlockWorkload::insert_giant_1line:
        return giant_line();
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
} // namespace nenenib::tests::performance
