// scope `--wsl-places` の単体テスト（ADR 0162 の追補 #1450）。フォルダ選びの箱に足す WSL の
// 場所は core の純関数 wsl_places の 1 本が決める。入れ終わったディストリだけ・登録の順・使えない
// 名前と同じ名前の 2 つ目は外す・WSL が無ければ空。箱は出さない（実機の箱は施主の試用）。
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "WslDistribution.hpp"
#include "WslPlaces.hpp"

#include <array>
#include <string>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;

void verify_installed_in_order()
{
    const std::array<core::WslDistribution, 4> listed{
        {{"Ubuntu", core::wsl_installed_state},
         {"Ubuntu-24.04", core::wsl_installed_state},
         {"docker-desktop", core::wsl_installed_state},
         {"Debian", 3}}};
    const std::vector<std::string> expected{R"(\\wsl.localhost\Ubuntu)",
                                            R"(\\wsl.localhost\Ubuntu-24.04)",
                                            R"(\\wsl.localhost\docker-desktop)"};
    expect(core::wsl_places(listed) == expected,
           "installed distributions become places in the registry order");
    expect(core::wsl_places({}).empty(), "no WSL adds no place");
}

void verify_unusable_names()
{
    const std::array<core::WslDistribution, 9> listed{{{"", core::wsl_installed_state},
                                                       {".", core::wsl_installed_state},
                                                       {"..", core::wsl_installed_state},
                                                       {R"(a\b)", core::wsl_installed_state},
                                                       {"a/b", core::wsl_installed_state},
                                                       {"a:b", core::wsl_installed_state},
                                                       {"a\tb", core::wsl_installed_state},
                                                       {"a?b", core::wsl_installed_state},
                                                       {"Arch", 0}}};
    expect(core::wsl_places(listed).empty(),
           "empty, dot, separator, reserved and control names and uninstalled ones are left out");
}

void verify_duplicates_and_text()
{
    const std::array<core::WslDistribution, 3> listed{{{"Ubuntu", core::wsl_installed_state},
                                                       {"日本語", core::wsl_installed_state},
                                                       {"Ubuntu", core::wsl_installed_state}}};
    const std::vector<std::string> expected{R"(\\wsl.localhost\Ubuntu)",
                                            R"(\\wsl.localhost\日本語)"};
    expect(core::wsl_places(listed) == expected,
           "the second of the same name is left out and UTF-8 names are kept");
}
} // namespace

void verify_wsl_places_scope()
{
    verify_installed_in_order();
    verify_unusable_names();
    verify_duplicates_and_text();
}
} // namespace nenenib::tests
