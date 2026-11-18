#include "FileUri.hpp"

#include "AsciiCase.hpp"
#include "FileUrl.hpp"
#include "FileUrlReading.hpp"

#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::string_view file_scheme = "file:";
} // namespace

std::optional<FilePath> file_path_of_uri(std::string_view uri)
{
    if (!ascii_prefix_ignoring_case(uri, file_scheme))
    {
        return std::nullopt;
    }
    const auto native =
        file_url_path(uri.substr(file_scheme.size()), FileUrlReading::language_server);
    if (!native.has_value())
    {
        return std::nullopt;
    }
    auto path = FilePath::parse(native.value());
    return path.has_value() ? std::optional<FilePath>{std::move(path).value()} : std::nullopt;
}
} // namespace nenenib::core
