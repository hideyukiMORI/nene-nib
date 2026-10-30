#include "WindowLines.hpp"

#include "ExplorerLayout.hpp"

namespace nenenib::ui::win32
{
application::VisibleLines window_lines(const core::BodyLayout &body, core::LayoutRect client,
                                       std::uint32_t dpi, core::FontSize font_size) noexcept
{
    return application::VisibleLines{body.visible_lines,
                                     {},
                                     core::width_of(client),
                                     core::height_of(client),
                                     core::explorer_panel_lines(client, dpi, font_size)};
}
} // namespace nenenib::ui::win32
