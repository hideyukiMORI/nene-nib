#include "FinderPreview.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace nenenib::core
{
namespace
{
// 名前の表（FinderPreview の宣言の順）。
constexpr std::array<std::string_view, 4> preview_names{"auto", "right", "below", "off"};
constexpr std::array<FinderPreview, 4> preview_values{
    FinderPreview::automatic, FinderPreview::right, FinderPreview::below, FinderPreview::off};

// 描いている形の次（right → below → off → right）。
[[nodiscard]] FinderPreview after(FinderPreview drawn) noexcept
{
    switch (drawn)
    {
    case FinderPreview::right:
        return FinderPreview::below;
    case FinderPreview::below:
        return FinderPreview::off;
    case FinderPreview::automatic:
    case FinderPreview::off:
        return FinderPreview::right;
    }
    std::unreachable();
}
} // namespace

std::string_view finder_preview_name(FinderPreview preview) noexcept
{
    return preview_names.at(std::to_underlying(preview));
}

std::optional<FinderPreview> finder_preview_named(std::string_view name) noexcept
{
    for (std::size_t index = 0; index < preview_names.size(); ++index)
    {
        if (preview_names.at(index) == name)
        {
            return preview_values.at(index);
        }
    }
    return std::nullopt;
}

FinderPreview finder_preview_cycled(FinderPreview drawn) noexcept
{
    return after(drawn);
}

bool palette_previews(PaletteScope scope) noexcept
{
    switch (scope)
    {
    case PaletteScope::files:
    case PaletteScope::tabs:
    case PaletteScope::bookmarks:
    case PaletteScope::history:
    case PaletteScope::folder:
    case PaletteScope::project_hits:
    case PaletteScope::diagnostics:
    case PaletteScope::references:
    case PaletteScope::symbols:
        return true;
    case PaletteScope::commands:
    case PaletteScope::operations:
        return false;
    }
    std::unreachable();
}

FinderPreview palette_placement(PaletteScope scope, FinderPreview chosen) noexcept
{
    return palette_previews(scope) ? chosen : FinderPreview::off;
}
} // namespace nenenib::core
