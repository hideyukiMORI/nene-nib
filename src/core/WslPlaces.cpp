#include "WslPlaces.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace nenenib::core
{
namespace
{
constexpr std::string_view wsl_share_root = R"(\\wsl.localhost\)";
// Windows の経路の 1 節に置けない文字（区切りを含む）。制御文字は別に見る。
constexpr std::string_view unusable_characters = R"(\/:*?"<>|)";
// Docker Desktop が内部で使うディストリ。使う人が開くフォルダではないので場所にしない。
// 前方一致にはしない（使う人が `docker-desktop-dev` のような名前を付ける余地を残す）。
constexpr std::array<std::string_view, 2> internal_distributions{"docker-desktop",
                                                                 "docker-desktop-data"};

[[nodiscard]] char ascii_lower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

// UNC の経路も WSL の名前も大小を区別しないので、ASCII の大小を無視して比べる。
[[nodiscard]] bool same_name(std::string_view left, std::string_view right)
{
    return std::ranges::equal(left, right,
                              [](char a, char b) { return ascii_lower(a) == ascii_lower(b); });
}

[[nodiscard]] bool listed_in(std::span<const std::string_view> names, std::string_view name)
{
    return std::ranges::any_of(names,
                               [name](std::string_view other) { return same_name(other, name); });
}

[[nodiscard]] bool usable_name(std::string_view name)
{
    if (name.empty() || name.size() > wsl_name_limit || name == "." || name == "..")
    {
        return false;
    }
    return std::ranges::none_of(
        name, [](char c)
        { return static_cast<unsigned char>(c) < 0x20U || unusable_characters.contains(c); });
}

// 場所にしてよいか。入れ終わった・使える名前・内部の名前でない・まだ選んでいない。
[[nodiscard]] bool placeable(const WslDistribution &distribution,
                             std::span<const std::string_view> chosen)
{
    const std::string_view name = distribution.name;
    return distribution.state == wsl_installed_state && usable_name(name) &&
           !listed_in(internal_distributions, name) && !listed_in(chosen, name);
}
} // namespace

std::vector<std::string> wsl_places(std::span<const WslDistribution> distributions)
{
    std::vector<std::string_view> chosen;
    for (const auto &distribution : distributions)
    {
        if (chosen.size() == wsl_place_limit)
        {
            break;
        }
        if (placeable(distribution, chosen))
        {
            chosen.emplace_back(distribution.name);
        }
    }
    std::vector<std::string> places;
    places.reserve(chosen.size());
    for (const auto name : chosen)
    {
        places.push_back(std::string(wsl_share_root).append(name));
    }
    return places;
}
} // namespace nenenib::core
