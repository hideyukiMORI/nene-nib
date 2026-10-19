#include "FontFamilies.hpp"

#include "DisplayText.hpp"
#include "ExResult.hpp"

#include <algorithm>
#include <cstddef>
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

// 並びは adapters が OS の照合順で整えたまま変えない（#696・ARC-007）。重複は最初の 1 つを残す。
FontFamilies FontFamilies::from(std::vector<std::string> names)
{
    std::erase_if(names,
                  [](const std::string &name)
                  {
                      return name.empty() ||
                             !DisplayText::parse(std::string(guifont_command) + name).has_value();
                  });
    std::vector<std::size_t> order(names.size());
    for (std::size_t index = 0; index < order.size(); ++index)
    {
        order.at(index) = index;
    }
    std::sort(order.begin(), order.end(),
              [&names](std::size_t left, std::size_t right)
              {
                  const auto compared = names.at(left).compare(names.at(right));
                  return compared != 0 ? compared < 0 : left < right;
              });
    std::vector<bool> repeated(names.size(), false);
    for (std::size_t index = 1; index < order.size(); ++index)
    {
        repeated.at(order.at(index)) = names.at(order.at(index)) == names.at(order.at(index - 1));
    }
    std::vector<std::string> kept;
    kept.reserve(names.size());
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        if (!repeated.at(index))
        {
            kept.push_back(std::move(names.at(index)));
        }
    }
    return FontFamilies(std::make_shared<const std::vector<std::string>>(std::move(kept)));
}

std::span<const std::string> FontFamilies::names() const & noexcept
{
    return *names_;
}
} // namespace nenenib::core
