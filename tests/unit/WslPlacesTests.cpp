// scope `--wsl-places` の単体テスト（ADR 0162 の追補 #1450）。フォルダ選びの箱に足す WSL の
// 場所は core の純関数 wsl_places の 1 本が決める。入れ終わったディストリだけ・登録の順・上限
// 16 件・使えない名前・Docker の内部の名前・同じ名前の 2 つ目（ASCII の大小を無視）は外す・WSL が
// 無ければ空。箱は出さない（実機の箱は施主の試用）。
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "WslDistribution.hpp"
#include "WslPlaces.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;

[[nodiscard]] core::WslDistribution installed(std::string name)
{
    return {std::move(name), core::wsl_installed_state};
}

void verify_installed_in_order()
{
    const std::array<core::WslDistribution, 4> listed{
        {installed("Ubuntu"), installed("Ubuntu-24.04"), installed("Ubuntu-22.04"), {"Debian", 3}}};
    const std::vector<std::string> expected{R"(\\wsl.localhost\Ubuntu)",
                                            R"(\\wsl.localhost\Ubuntu-24.04)",
                                            R"(\\wsl.localhost\Ubuntu-22.04)"};
    expect(core::wsl_places(listed) == expected,
           "installed distributions become places in the registry order");
    expect(core::wsl_places({}).empty(), "no WSL adds no place");
}

void verify_docker_internal()
{
    const std::array<core::WslDistribution, 5> listed{
        {installed("Ubuntu"), installed("docker-desktop"), installed("docker-desktop-data"),
         installed("Docker-Desktop"), installed("docker-desktop-dev")}};
    const std::vector<std::string> expected{R"(\\wsl.localhost\Ubuntu)",
                                            R"(\\wsl.localhost\docker-desktop-dev)"};
    expect(core::wsl_places(listed) == expected,
           "docker-desktop and docker-desktop-data are left out in any ASCII case, "
           "other names that start with them are kept");
}

void verify_unusable_names()
{
    const std::array<core::WslDistribution, 6> listed{{installed(""),
                                                       installed("."),
                                                       installed(".."),
                                                       installed("a\tb"),
                                                       installed(std::string(1, '\0')),
                                                       {"Arch", 0}}};
    expect(core::wsl_places(listed).empty(),
           "empty, dot, control names and uninstalled ones are left out");
    for (const char c : std::string_view(R"(\/:*?"<>|)"))
    {
        const std::array<core::WslDistribution, 1> one{installed(std::string("a") + c + "b")};
        expect(core::wsl_places(one).empty(), "a name with a path-unusable character is left out");
    }
}

void verify_name_length()
{
    const std::array<core::WslDistribution, 2> listed{
        {installed(std::string(core::wsl_name_limit, 'a')),
         installed(std::string(core::wsl_name_limit + 1, 'b'))}};
    const auto places = core::wsl_places(listed);
    expect(places.size() == 1 && places.front().ends_with(std::string(core::wsl_name_limit, 'a')),
           "a name up to the limit is kept and a longer one is left out");
}

void verify_place_limit()
{
    std::vector<core::WslDistribution> listed;
    for (std::size_t index = 0; index < core::wsl_place_limit + 4; ++index)
    {
        listed.push_back(installed("D" + std::to_string(index)));
    }
    const auto places = core::wsl_places(listed);
    expect(places.size() == core::wsl_place_limit &&
               places.back() == R"(\\wsl.localhost\D)" + std::to_string(core::wsl_place_limit - 1),
           "places stop at the limit in the registry order");
}

void verify_duplicates_and_text()
{
    const std::array<core::WslDistribution, 4> listed{
        {installed("Ubuntu"), installed("日本語"), installed("Ubuntu"), installed("UBUNTU")}};
    const std::vector<std::string> expected{R"(\\wsl.localhost\Ubuntu)",
                                            R"(\\wsl.localhost\日本語)"};
    expect(core::wsl_places(listed) == expected,
           "the second of the same name in any ASCII case is left out and UTF-8 names are kept");
}
} // namespace

void verify_wsl_places_scope()
{
    verify_installed_in_order();
    verify_docker_internal();
    verify_unusable_names();
    verify_name_length();
    verify_place_limit();
    verify_duplicates_and_text();
}
} // namespace nenenib::tests
