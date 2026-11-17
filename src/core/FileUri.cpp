#include "FileUri.hpp"

#include "PercentDecoding.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::string_view file_scheme = "file:";
constexpr std::string_view authority_lead = "//";
constexpr std::string_view localhost = "localhost";
constexpr std::size_t drive_length = 2;

[[nodiscard]] char ascii_lower(char value) noexcept
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

[[nodiscard]] bool same_ignoring_case(std::string_view left, std::string_view right) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (ascii_lower(left[index]) != ascii_lower(right[index]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool ascii_letter(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

// 最初の `/` の位置（無ければ末尾）。
[[nodiscard]] std::size_t first_slash(std::string_view text) noexcept
{
    std::size_t index = 0;
    while (index < text.size() && text[index] != '/')
    {
        ++index;
    }
    return index;
}

// 区切りの `/` を `\` にして足す。
void append_native(std::string &path, std::string_view text)
{
    for (const char character : text)
    {
        path.push_back(character == '/' ? '\\' : character);
    }
}

// host の無い（か localhost の）URI のパスの部分 `/C:/x` → `C:\x`。ドライブが無ければ値なし。
[[nodiscard]] std::optional<std::string> drive_path(std::string_view decoded)
{
    const std::string_view path =
        !decoded.empty() && decoded.front() == '/' ? decoded.substr(1) : decoded;
    if (path.size() < drive_length || !ascii_letter(path[0]) || path[1] != ':')
    {
        return std::nullopt;
    }
    std::string native;
    native.reserve(path.size());
    append_native(native, path);
    return native;
}
} // namespace

std::optional<FilePath> file_path_of_uri(std::string_view uri)
{
    if (uri.size() < file_scheme.size() ||
        !same_ignoring_case(uri.substr(0, file_scheme.size()), file_scheme))
    {
        return std::nullopt;
    }
    std::string_view rest = uri.substr(file_scheme.size());
    std::string_view host;
    if (rest.substr(0, authority_lead.size()) == authority_lead)
    {
        rest.remove_prefix(authority_lead.size());
        const std::size_t end = first_slash(rest);
        host = rest.substr(0, end);
        rest.remove_prefix(end);
    }
    const std::string decoded = percent_decoded(rest);
    std::optional<std::string> native;
    if (host.empty() || same_ignoring_case(host, localhost))
    {
        native = drive_path(decoded);
    }
    else
    {
        std::string unc{"\\\\"};
        unc += percent_decoded(host);
        append_native(unc, decoded);
        native = std::move(unc);
    }
    if (!native.has_value())
    {
        return std::nullopt;
    }
    auto path = FilePath::parse(native.value());
    return path.has_value() ? std::optional<FilePath>{std::move(path).value()} : std::nullopt;
}
} // namespace nenenib::core
