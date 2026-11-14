#include "AppIdentity.hpp"

namespace nenenib::core
{
namespace
{
constexpr std::string_view copyright_sign = "© ";
constexpr std::string_view separator = " · ";
} // namespace

// NENENIB_APP_VERSION は CMakeLists.txt が project(... VERSION) の PROJECT_VERSION をこの翻訳単位に
// だけ渡す compile definition（CNF-007 が渡し方を照合する）。
std::string_view app_version() noexcept
{
    return NENENIB_APP_VERSION;
}

std::string app_copyright_line()
{
    std::string line(copyright_sign);
    line += app_copyright_year;
    line += ' ';
    line += app_author;
    return line;
}

std::string app_about_line()
{
    std::string line(app_name);
    line += ' ';
    line += app_version();
    line += separator;
    line += app_copyright_line();
    line += separator;
    line += app_homepage;
    return line;
}
} // namespace nenenib::core
