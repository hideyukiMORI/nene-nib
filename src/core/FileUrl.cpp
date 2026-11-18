#include "FileUrl.hpp"

#include "AsciiCase.hpp"
#include "PercentDecoding.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
constexpr char native_separator = '\\';
constexpr std::string_view localhost = "localhost";
constexpr std::string_view unc_lead = "\\\\";
constexpr std::string_view host_forbidden = "/\\:*?\"<>|";
constexpr std::size_t drive_length = 2;
constexpr char lower_to_upper = 'a' - 'A';

[[nodiscard]] bool ascii_letter(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

// at から次の区切りの位置（無ければ text の大きさ）。
[[nodiscard]] std::size_t next_separator(std::string_view text, std::size_t at) noexcept
{
    while (at < text.size() && !path_separator(text[at]))
    {
        ++at;
    }
    return at;
}

// 段を 1 つ積む。`.` と空の段は落とし、`..` は 1 つ上へ畳む（根より上へは出ずに印を立てる）。
void push_part(std::vector<std::string_view> &parts, std::string_view part, bool &above_root)
{
    if (part == "..")
    {
        if (parts.empty())
        {
            above_root = true;
            return;
        }
        parts.pop_back();
        return;
    }
    if (!part.empty() && part != ".")
    {
        parts.push_back(part);
    }
}

[[nodiscard]] bool forbidden_in_host(char value) noexcept
{
    for (const char forbidden : host_forbidden)
    {
        if (value == forbidden)
        {
            return true;
        }
    }
    return false;
}

// UNC の host（解いた後）の形。device の名前空間（`.` `?`）は
// device、空・`..`・パスに使えない文字を 含む host は malformed。
[[nodiscard]] std::expected<void, FileUrlRefusal> host_shape(std::string_view host) noexcept
{
    if (host == "." || host == "?")
    {
        return std::unexpected{FileUrlRefusal::device};
    }
    if (host.empty() || host == "..")
    {
        return std::unexpected{FileUrlRefusal::malformed};
    }
    for (const char character : host)
    {
        if (forbidden_in_host(character))
        {
            return std::unexpected{FileUrlRefusal::malformed};
        }
    }
    return {};
}

// language_server の host のある URL → `\\host\share\x`（share は必須）。
[[nodiscard]] std::expected<std::string, FileUrlRefusal> unc_path(std::string_view host,
                                                                  std::string_view path)
{
    const std::string name = percent_decoded(host);
    const auto shape = host_shape(name);
    if (!shape)
    {
        return std::unexpected{shape.error()};
    }
    const std::string decoded = percent_decoded(path);
    const std::string_view rest = decoded;
    const std::size_t share_end = rest.empty() ? 0 : next_separator(rest, 1U);
    if (rest.empty() || !path_separator(rest[0]) || share_end == 1U)
    {
        return std::unexpected{FileUrlRefusal::malformed};
    }
    std::string root{unc_lead};
    root += name;
    root.push_back(native_separator);
    root.append(rest.substr(1U, share_end - 1U));
    const std::string_view share = rest.substr(1U, share_end - 1U);
    if (share == "." || share == "..")
    {
        return std::unexpected{FileUrlRefusal::malformed};
    }
    auto collapsed = collapsed_path(root, rest.substr(share_end));
    if (collapsed.above_root)
    {
        return std::unexpected{FileUrlRefusal::above_root};
    }
    return std::move(collapsed.text);
}

// host の無い（か localhost の）URL のパス `/C:/x` → `C:\x`。
[[nodiscard]] std::expected<std::string, FileUrlRefusal> drive_path(std::string_view path,
                                                                    FileUrlReading reading)
{
    const std::string decoded = percent_decoded(path);
    std::string_view local = decoded;
    if (!local.empty() && path_separator(local[0]) && drive_absolute_path(local.substr(1)))
    {
        local = local.substr(1);
    }
    if (!drive_absolute_path(local))
    {
        return std::unexpected{FileUrlRefusal::malformed};
    }
    auto collapsed = collapsed_path(local.substr(0, drive_length), local.substr(drive_length + 1U));
    switch (reading)
    {
    case FileUrlReading::image_reference:
        return std::move(collapsed.text);
    case FileUrlReading::language_server:
        if (collapsed.above_root)
        {
            return std::unexpected{FileUrlRefusal::above_root};
        }
        if (collapsed.text[0] >= 'a' && collapsed.text[0] <= 'z')
        {
            collapsed.text[0] = static_cast<char>(collapsed.text[0] - lower_to_upper);
        }
        return std::move(collapsed.text);
    }
    return std::unexpected{FileUrlRefusal::malformed};
}
} // namespace

bool path_separator(char value) noexcept
{
    return value == '/' || value == '\\';
}

std::string_view url_without_suffix(std::string_view text) noexcept
{
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] == '?' || text[index] == '#')
        {
            return text.substr(0, index);
        }
    }
    return text;
}

bool drive_absolute_path(std::string_view text) noexcept
{
    return text.size() > drive_length && ascii_letter(text[0]) && text[1] == ':' &&
           path_separator(text[2]);
}

CollapsedPath collapsed_path(std::string_view root, std::string_view rest)
{
    std::vector<std::string_view> parts;
    bool above_root = false;
    std::size_t start = 0;
    for (std::size_t index = 0; index <= rest.size(); ++index)
    {
        if (index == rest.size() || path_separator(rest[index]))
        {
            push_part(parts, rest.substr(start, index - start), above_root);
            start = index + 1U;
        }
    }
    std::string text{root};
    text.push_back(native_separator);
    for (std::size_t index = 0; index < parts.size(); ++index)
    {
        if (index != 0U)
        {
            text.push_back(native_separator);
        }
        text.append(parts[index]);
    }
    return CollapsedPath{.text = std::move(text), .above_root = above_root};
}

std::expected<std::string, FileUrlRefusal> file_url_path(std::string_view rest,
                                                         FileUrlReading reading)
{
    std::string_view path = url_without_suffix(rest);
    std::string_view host;
    if (path.size() >= 2U && path[0] == '/' && path[1] == '/')
    {
        const std::size_t host_end = next_separator(path, 2U);
        host = path.substr(2U, host_end - 2U);
        path = path.substr(host_end);
    }
    const bool local = host.empty() || (host.size() == localhost.size() &&
                                        ascii_prefix_ignoring_case(host, localhost));
    if (local)
    {
        return drive_path(path, reading);
    }
    switch (reading)
    {
    case FileUrlReading::image_reference:
        return std::unexpected{FileUrlRefusal::network};
    case FileUrlReading::language_server:
        return unc_path(host, path);
    }
    return std::unexpected{FileUrlRefusal::malformed};
}
} // namespace nenenib::core
