#include "EasyMotionLabels.hpp"

#include "DisplayWidth.hpp"
#include "EasyMotionState.hpp"
#include "LineNumber.hpp"
#include "MotionLabel.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::application
{
namespace
{
constexpr std::uint8_t narrow_cells = 1;
constexpr std::uint8_t wide_cells = 2;

// 跳び先の文字の升の数（全角は 2・ほかは 1）。
[[nodiscard]] std::uint8_t cells_of(char32_t code) noexcept
{
    return core::display_width(code) == core::DisplayWidth::wide ? wide_cells : narrow_cells;
}

// line の表示の中の at バイト目までの code point の数 + 1（行頭が Column{1}）。
[[nodiscard]] core::Column column_at(std::string_view text, std::size_t at) noexcept
{
    std::size_t count = 0;
    for (core::Offset walk{0}; walk.value < at && walk.value < text.size();
         walk = core::next_code_point(text, walk))
    {
        ++count;
    }
    return core::Column{count + 1};
}

// 1 つの印を、跳び先が載る見えている行へ写す（行の中の本文の始まりは行頭 + 窓のバイト数）。
void place_label(const core::TextBuffer &text, core::Offset target, std::string label,
                 std::vector<LineView> &lines)
{
    const core::LineNumber line = text.line_containing(target);
    for (LineView &view : lines)
    {
        if (!(view.number == line))
        {
            continue;
        }
        const std::size_t begin = text.line_start(line).value + view.window.byte;
        // 行の終わり（空の行の行頭を含む）の跳び先も載せる（桁は行の code point の数 + 1・1 升）。
        if (target.value < begin || target.value > begin + view.text.size())
        {
            return;
        }
        const std::size_t at = target.value - begin;
        const std::uint8_t cells = at < view.text.size()
                                       ? cells_of(core::code_point_at(view.text, core::Offset{at}))
                                       : narrow_cells;
        view.labels.push_back(core::MotionLabel{column_at(view.text, at), std::move(label), cells});
        return;
    }
}
} // namespace

void mark_labels(const EditorState &state, std::vector<LineView> &lines)
{
    const auto &shown = state.vim().easy_motion;
    if (!shown.has_value() || state.mode() != core::EditMode::vim)
    {
        return;
    }
    const core::EasyMotionState &labels = shown.value();
    const std::string_view typed = labels.typed;
    for (std::size_t index = 0; index < labels.targets.size() && index < labels.labels.size();
         ++index)
    {
        const std::string &label = labels.labels.at(index);
        if (label.starts_with(typed) && label.size() > typed.size())
        {
            place_label(state.vim_text(), labels.targets.at(index), label.substr(typed.size()),
                        lines);
        }
    }
}
} // namespace nenenib::application
