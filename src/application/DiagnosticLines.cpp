#include "DiagnosticLines.hpp"

#include "ComposedSpans.hpp"
#include "DiagnosticEdits.hpp"
#include "GiantLineWindow.hpp"
#include "Utf8.hpp"

#include <cstddef>
#include <string>

namespace nenenib::application
{
void diagnosed_line(const EditorState &state, LineView &view)
{
    const auto &diagnostics = state.diagnostics();
    if (diagnostics.empty() || view.window.active)
    {
        return;
    }
    auto spans =
        core::diagnostic_spans(view.text, state.text().line_start(view.number), diagnostics);
    view.diagnostics =
        view.composed.has_value()
            ? composed_spans(spans, composed_source_column(view.display, view.composed.value()),
                             view.composed.value().inserted_columns)
            : std::move(spans);
}

void grid_diagnosed_line(const EditorState &state, core::LineNumber line,
                         std::vector<GridDiagnosticRange> &diagnostics)
{
    const auto &text = state.text();
    if (state.diagnostics().empty() || giant_line_index(text, line) != nullptr)
    {
        return;
    }
    const auto start = text.line_start(line);
    const std::string body = text.text_range(start, text.line_end(line));
    // 区間は昇順なので、桁と位置を 1 回だけ前へ歩いて写す（grid_colored_line と同じ）。
    std::size_t byte = 0;
    std::size_t column = 1;
    const auto offset_of = [&byte, &column, &body, start](core::Column wanted)
    {
        for (; column < wanted.value && byte < body.size(); ++column)
        {
            byte = core::next_code_point(body, core::Offset{byte}).value;
        }
        return core::Offset{start.value + byte};
    };
    for (const auto &span : core::diagnostic_spans(body, start, state.diagnostics()))
    {
        const auto begin = offset_of(span.begin);
        diagnostics.push_back(
            GridDiagnosticRange{core::OffsetRange{begin, offset_of(span.end)}, span.severity});
    }
}
} // namespace nenenib::application
