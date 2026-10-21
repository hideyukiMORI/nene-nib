#include "HtmlClipboardFormat.hpp"

#include <cstddef>

namespace nenenib::adapters::win32
{
namespace
{
constexpr std::string_view start_key = "StartFragment:";
constexpr std::string_view end_key = "EndFragment:";
// 位置の桁の上限（size_t の 10 進の桁に収める）。見出しの数は 10 桁（ゼロ埋め）が普通。
constexpr std::size_t digit_limit = 18;

// 見出しの部分（最初の `<` より前）から key の後ろの 10 進の数を読む。
[[nodiscard]] std::optional<std::size_t> header_number(std::string_view header,
                                                       std::string_view key) noexcept
{
    const std::size_t found = header.find(key);
    if (found == std::string_view::npos)
    {
        return std::nullopt;
    }
    std::size_t at = found + key.size();
    std::size_t value = 0;
    std::size_t digits = 0;
    while (at < header.size() && header[at] >= '0' && header[at] <= '9' && digits < digit_limit)
    {
        value = (value * 10U) + static_cast<std::size_t>(header[at] - '0');
        ++at;
        ++digits;
    }
    if (digits == 0)
    {
        return std::nullopt;
    }
    return value;
}
} // namespace

std::optional<std::string_view> html_fragment_of(std::string_view clipboard_bytes)
{
    const std::string_view header = clipboard_bytes.substr(0, clipboard_bytes.find('<'));
    const auto begin = header_number(header, start_key);
    const auto end = header_number(header, end_key);
    if (!begin.has_value() || !end.has_value() || begin.value() > end.value() ||
        end.value() > clipboard_bytes.size())
    {
        return std::nullopt;
    }
    return clipboard_bytes.substr(begin.value(), end.value() - begin.value());
}
} // namespace nenenib::adapters::win32
