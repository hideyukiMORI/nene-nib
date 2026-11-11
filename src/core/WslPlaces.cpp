#include "WslPlaces.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::string_view wsl_share_root = R"(\\wsl.localhost\)";
// Windows の経路の 1 節に置けない文字（区切りを含む）。制御文字は別に見る。
constexpr std::string_view unusable_characters = R"(\/:*?"<>|)";

[[nodiscard]] bool usable_name(std::string_view name)
{
    if (name.empty() || name == "." || name == "..")
    {
        return false;
    }
    return std::ranges::none_of(
        name, [](char c)
        { return static_cast<unsigned char>(c) < 0x20U || unusable_characters.contains(c); });
}
} // namespace

std::vector<std::string> wsl_places(std::span<const WslDistribution> distributions)
{
    std::vector<std::string> places;
    for (const auto &distribution : distributions)
    {
        if (distribution.state != wsl_installed_state || !usable_name(distribution.name))
        {
            continue;
        }
        auto place = std::string(wsl_share_root) + distribution.name;
        if (std::ranges::find(places, place) == places.end())
        {
            places.push_back(std::move(place));
        }
    }
    return places;
}
} // namespace nenenib::core
