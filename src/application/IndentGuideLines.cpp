// インデントガイドの欄（ADR 0193 の決定 6・7・9・14・15・#1975）。
#include "IndentGuideLines.hpp"

#include "EditorFeature.hpp"
#include "EditorState.hpp"
#include "FeatureCosts.hpp"
#include "IndentGuide.hpp"
#include "IndentGuideRun.hpp"
#include "IndentGuides.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"
#include "TextBuffer.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <vector>

namespace nenenib::application
{
namespace
{
// 見えている行の字下げ（空の行は値なし）。巨大な行の窓は窓の文字列でなく本文の行頭を読む。
[[nodiscard]] std::optional<std::size_t> shown_indent(const core::TextBuffer &text,
                                                      const LineView &line)
{
    if (line.window.active)
    {
        return core::line_indent_cells(text, line.number);
    }
    if (core::blank_indent_line(line.text))
    {
        return std::nullopt;
    }
    return core::indent_cells(line.text);
}

// line から上（upward）か下へ screen 行まで、最も近い空でない行の字下げ。
[[nodiscard]] std::optional<std::size_t>
nearest_indent(const core::TextBuffer &text, core::LineNumber line, bool upward, std::size_t screen)
{
    const std::size_t last = text.line_count();
    std::size_t number = line.value;
    for (std::size_t step = 0; step < screen; ++step)
    {
        if ((upward && number <= 1) || (!upward && number >= last))
        {
            return std::nullopt;
        }
        number = upward ? number - 1 : number + 1;
        const auto cells = core::line_indent_cells(text, core::LineNumber{number});
        if (cells.has_value())
        {
            return cells;
        }
    }
    return std::nullopt;
}
// 行ごとの段の数（欄の無い行は 0・強める区間はそこで切れる）。
[[nodiscard]] std::vector<std::size_t> shown_levels(const std::vector<LineView> &lines)
{
    std::vector<std::size_t> levels;
    levels.reserve(lines.size());
    for (const LineView &line : lines)
    {
        levels.push_back(line.indent_guides.has_value() ? line.indent_guides.value().levels : 0);
    }
    return levels;
}

void strengthen(std::vector<LineView> &lines, const core::IndentGuideRun &run)
{
    for (std::size_t index = run.begin; index < run.end; ++index)
    {
        auto &guides = lines.at(index).indent_guides;
        if (guides.has_value())
        {
            guides.value().active = run.level;
        }
    }
}
} // namespace

void mark_indent_guides(const EditorState &state, std::size_t screen, std::vector<LineView> &lines)
{
    if (lines.empty() ||
        !core::feature_enabled(state.settings(), core::EditorFeature::indent_guides))
    {
        return;
    }
    const core::TextBuffer &text = state.text();
    std::vector<std::optional<std::size_t>> cells;
    cells.reserve(lines.size());
    for (const LineView &line : lines)
    {
        cells.push_back(shown_indent(text, line));
    }
    const std::size_t reach = std::max<std::size_t>(screen, 1);
    const auto before = cells.front().has_value()
                            ? std::nullopt
                            : nearest_indent(text, lines.front().number, true, reach);
    const auto after = cells.back().has_value()
                           ? std::nullopt
                           : nearest_indent(text, lines.back().number, false, reach);
    const auto continued = core::continued_indents(cells, before, after);
    const std::size_t step =
        core::indent_guide_step(state.document().indent_unit, state.settings().indent_guide_width);
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        LineView &line = lines.at(index);
        if (line.window.active || line.composed.has_value())
        {
            continue;
        }
        line.indent_guides = IndentGuides{continued.at(index), step, continued.at(index) / step};
    }
}

void mark_active_indent_guide(const EditorState &state, std::vector<LineView> &lines)
{
    if (lines.empty() || !state.settings().indent_guide_active ||
        !core::feature_enabled(state.settings(), core::EditorFeature::indent_guides))
    {
        return;
    }
    const core::LineNumber caret = state.text().position_of(state.selection().caret).line;
    const auto shown =
        std::ranges::find_if(lines, [caret](const LineView &line) { return line.number == caret; });
    if (shown == lines.end() || !shown->indent_guides.has_value())
    {
        return;
    }
    const auto run = core::active_indent_run(
        shown_levels(lines), static_cast<std::size_t>(std::distance(lines.begin(), shown)));
    if (run.has_value())
    {
        strengthen(lines, run.value());
    }
}
} // namespace nenenib::application
