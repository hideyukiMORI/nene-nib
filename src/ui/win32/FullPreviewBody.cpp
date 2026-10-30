#include "FullPreviewBody.hpp"

#include "PaneLayouts.hpp"
#include "PaneSide.hpp"
#include "SplitLayout.hpp"

namespace nenenib::ui::win32
{
core::BodyLayout full_preview_body(const core::LayoutRect &area, std::uint32_t dpi,
                                   core::FontSize font_size) noexcept
{
    return core::active_pane_layout(
        core::pane_layouts(area, dpi, font_size, core::SplitLayout::none), core::PaneSide::first);
}
} // namespace nenenib::ui::win32
