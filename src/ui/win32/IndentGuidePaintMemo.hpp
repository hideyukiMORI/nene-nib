#pragma once

#include "EditorFeature.hpp"
#include "EditorSettings.hpp"
#include "FeatureCosts.hpp"
#include "IndentGuide.hpp"
#include "IndentGuidePaint.hpp"
#include "Palette.hpp"
#include "SyntaxPalette.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>

namespace nenenib::ui::win32
{
// インデントガイドの色の材料の覚え（ADR 0193 の決定 5・6・#1975 のレビュー R2）。renderer が 1 つ
// 持ち、frame の初めに begin を呼ぶ。`features=indentguides` がオフなら真偽を 1 つ読んで描かない印
// だけを立てる。オンなら、単色 / 虹・色の数・rainbow_colors が読む本文の色（foreground と
// rainbow_roles）・線の 3 色（panel_border・muted・background）が前と同じなら前の値を使い、どれかが
// 変われば core の indent_guide_paint で作り直す（テーマ・設定を変えた frame）。renderer
// を作り直すと 空から始まる。
class IndentGuidePaintMemo final
{
  public:
    void begin(const core::EditorSettings &settings, const core::SyntaxPalette &syntax,
               const core::Palette &palette)
    {
        shown_ = core::feature_enabled(settings, core::EditorFeature::indent_guides);
        if (!shown_ || same_source(settings, syntax, palette))
        {
            return;
        }
        paint_ = core::indent_guide_paint(settings.indent_guide_style,
                                          settings.indent_guide_color_count, syntax, palette);
        syntax_ = syntax;
        wanted_ = settings.indent_guide_color_count;
        ++made_;
    }

    // その frame の色の材料。オフの frame は空。
    [[nodiscard]] const core::IndentGuidePaint *shown() const noexcept
    {
        return shown_ && paint_.has_value() ? &paint_.value() : nullptr;
    }

    // 作り直した回数（試験が覚えの効きを数える）。
    [[nodiscard]] std::size_t made() const noexcept
    {
        return made_;
    }

  private:
    [[nodiscard]] bool same_source(const core::EditorSettings &settings,
                                   const core::SyntaxPalette &syntax,
                                   const core::Palette &palette) const noexcept
    {
        if (!paint_.has_value())
        {
            return false;
        }
        const core::IndentGuidePaint &paint = paint_.value();
        return paint.style == settings.indent_guide_style &&
               wanted_ == settings.indent_guide_color_count &&
               paint.plain == palette.panel_border && paint.plain_strong == palette.muted &&
               paint.background == palette.background && syntax_.foreground == syntax.foreground &&
               std::ranges::all_of(core::rainbow_roles, [this, &syntax](const auto role)
                                   { return syntax_.*role == syntax.*role; });
    }

    std::optional<core::IndentGuidePaint> paint_{};
    core::SyntaxPalette syntax_{};
    std::size_t wanted_ = 0;
    std::size_t made_ = 0;
    bool shown_ = false;
};
} // namespace nenenib::ui::win32
