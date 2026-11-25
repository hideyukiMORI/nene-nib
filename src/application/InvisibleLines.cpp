// 不可視文字の欄（ADR 0193 の決定 6・10・14・15・#1977）。
#include "InvisibleLines.hpp"

#include "EditorFeature.hpp"
#include "EditorSettings.hpp"
#include "EditorState.hpp"
#include "FeatureCosts.hpp"
#include "Invisibles.hpp"
#include "LineInvisibles.hpp"
#include "LineView.hpp"
#include "TextBuffer.hpp"

#include <optional>
#include <vector>

namespace nenenib::application
{
void mark_invisibles(const EditorState &state, std::vector<LineView> &lines)
{
    const core::EditorSettings &settings = state.settings();
    if (!core::feature_enabled(settings, core::EditorFeature::list))
    {
        return;
    }
    const core::TextBuffer &text = state.vim_text();
    for (LineView &line : lines)
    {
        if (line.window.active || line.composed.has_value())
        {
            continue;
        }
        line.invisibles = LineInvisibles{
            core::invisible_spans(line.text, settings),
            settings.list_eol ? core::line_break_mark(text, line.number) : std::nullopt};
    }
}
} // namespace nenenib::application
