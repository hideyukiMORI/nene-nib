#include "FontFamilies.hpp"

#include "DisplayText.hpp"
#include "ExResult.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
FontFamilies::FontFamilies(std::shared_ptr<const std::vector<std::string>> names)
    : names_(std::move(names))
{
}

FontFamilies FontFamilies::none()
{
    return FontFamilies(std::make_shared<const std::vector<std::string>>());
}

FontFamilies FontFamilies::from(std::vector<std::string> names)
{
    std::erase_if(names,
                  [](const std::string &name)
                  {
                      return name.empty() ||
                             !DisplayText::parse(std::string(guifont_command) + name).has_value();
                  });
    std::ranges::sort(names);
    const auto repeated = std::ranges::unique(names);
    names.erase(repeated.begin(), repeated.end());
    return FontFamilies(std::make_shared<const std::vector<std::string>>(std::move(names)));
}

std::span<const std::string> FontFamilies::names() const & noexcept
{
    return *names_;
}
} // namespace nenenib::core
