#pragma once
#include "BodyTextLayouts.hpp"

#include <cstddef>
#include <expected>

namespace nenenib::tests::ui
{
// GridRowPainter の文字組みの替え玉（#546）。本物の BodyTextLayouts へ渡し、get の回数を数える。
class GridCountingLayouts final
{
  public:
    explicit GridCountingLayouts(nenenib::ui::win32::BodyTextLayouts &layouts) noexcept
        : layouts_(layouts)
    {
    }
    [[nodiscard]] std::expected<nenenib::ui::win32::BodyTextLayouts::TextLayout,
                                nenenib::application::TextLayoutFailure>
    get(const nenenib::application::TextLayoutRequest &request)
    {
        ++gets_;
        return layouts_.get(request);
    }
    [[nodiscard]] const nenenib::ui::win32::BodyTextLayout *glyphs_of(IDWriteTextLayout *layout,
                                                                      float left)
    {
        return layouts_.glyphs_of(layout, left);
    }
    [[nodiscard]] std::size_t gets() const noexcept
    {
        return gets_;
    }

  private:
    nenenib::ui::win32::BodyTextLayouts &layouts_;
    std::size_t gets_ = 0;
};
} // namespace nenenib::tests::ui
