#include "ThemeDocument.hpp"

namespace nenenib::core
{
Theme theme_view(const ThemeDocument &document) noexcept
{
    return Theme{document.name.text(), document.appearance, document.ui, document.body,
                 ThemeSource{document.source.author.text(), document.source.license.text(),
                             document.source.url.text()}};
}
} // namespace nenenib::core
