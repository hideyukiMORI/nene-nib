#include "DocumentUri.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace nenenib::application
{
namespace
{
constexpr std::string_view drive_prefix = "file:///";
constexpr std::string_view host_prefix = "file://";
constexpr std::string_view unc_lead = "\\\\";
constexpr std::array<char, 16> hex_digits{'0', '1', '2', '3', '4', '5', '6', '7',
                                          '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
constexpr unsigned high_shift = 4;
constexpr unsigned low_mask = 0x0F;

[[nodiscard]] constexpr bool kept_as_is(unsigned char byte) noexcept
{
    return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
           (byte >= '0' && byte <= '9') || byte == '-' || byte == '.' || byte == '_' ||
           byte == '~' || byte == '/' || byte == ':';
}

void append_encoded(std::string &uri, std::string_view path)
{
    for (const char character : path)
    {
        const auto byte = static_cast<unsigned char>(character == '\\' ? '/' : character);
        if (kept_as_is(byte))
        {
            uri.push_back(static_cast<char>(byte));
            continue;
        }
        uri.push_back('%');
        uri.push_back(hex_digits.at(byte >> high_shift));
        uri.push_back(hex_digits.at(byte & low_mask));
    }
}
} // namespace

std::string document_uri(const core::FilePath &path)
{
    const std::string_view text = path.text();
    const bool unc = text.starts_with(unc_lead);
    std::string uri{unc ? host_prefix : drive_prefix};
    uri.reserve(uri.size() + text.size());
    append_encoded(uri, unc ? text.substr(unc_lead.size()) : text);
    return uri;
}
} // namespace nenenib::application
