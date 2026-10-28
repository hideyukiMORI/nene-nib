#include "ServerFramer.hpp"

#include <charconv>
#include <cstdint>
#include <expected>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using application::ServerFailure;
// 見出しの後の本文の始まり（`\r\n\r\n` の直後）と本文の長さ。
using FrameExtent = std::pair<std::size_t, std::size_t>;

constexpr std::string_view header_end = "\r\n\r\n";
constexpr std::string_view line_end = "\r\n";
constexpr std::string_view length_name = "content-length";

char lower(char letter) noexcept
{
    return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
}

bool same_name(std::string_view name, std::string_view expected) noexcept
{
    if (name.size() != expected.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < name.size(); ++index)
    {
        if (lower(name[index]) != expected[index])
        {
            return false;
        }
    }
    return true;
}

std::string_view trimmed(std::string_view text) noexcept
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
    {
        text.remove_suffix(1);
    }
    return text;
}

// 見出しの 1 行が Content-Length なら、その値。ほかの見出しなら nullopt。
std::expected<std::optional<std::uint64_t>, ServerFailure> length_of_line(std::string_view line)
{
    const auto colon = line.find(':');
    if (colon == std::string_view::npos)
    {
        return std::unexpected(ServerFailure::read_failed);
    }
    if (!same_name(trimmed(line.substr(0, colon)), length_name))
    {
        return std::optional<std::uint64_t>{};
    }
    const auto value = trimmed(line.substr(colon + 1));
    std::uint64_t length = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), length);
    if (value.empty() || error != std::errc{} || end != value.data() + value.size())
    {
        return std::unexpected(ServerFailure::read_failed);
    }
    return std::optional<std::uint64_t>{length};
}

// 見出しの全体（最後の `\r\n\r\n` を除く）から本文の長さを読む。Content-Length が無ければ誤り。
std::expected<std::uint64_t, ServerFailure> content_length(std::string_view headers)
{
    std::optional<std::uint64_t> found;
    while (!headers.empty())
    {
        const auto end = headers.find(line_end);
        const auto line = headers.substr(0, end);
        headers = end == std::string_view::npos ? std::string_view{}
                                                : headers.substr(end + line_end.size());
        const auto length = length_of_line(line);
        if (!length.has_value())
        {
            return std::unexpected(length.error());
        }
        found = length.value().has_value() ? length.value() : found;
    }
    if (!found.has_value())
    {
        return std::unexpected(ServerFailure::read_failed);
    }
    return found.value();
}

// 先頭の 1 枠の位置。まだ足りなければ nullopt。
std::expected<std::optional<FrameExtent>, ServerFailure> next_frame(std::string_view bytes)
{
    const auto end = bytes.find(header_end);
    if (end == std::string_view::npos || end > server_header_limit)
    {
        if (bytes.size() > server_header_limit + header_end.size())
        {
            return std::unexpected(ServerFailure::read_failed);
        }
        return std::optional<FrameExtent>{};
    }
    const auto length = content_length(bytes.substr(0, end));
    if (!length.has_value())
    {
        return std::unexpected(length.error());
    }
    if (length.value() > server_message_limit)
    {
        return std::unexpected(ServerFailure::message_too_long);
    }
    const auto start = end + header_end.size();
    const auto size = static_cast<std::size_t>(length.value());
    if (bytes.size() - start < size)
    {
        return std::optional<FrameExtent>{};
    }
    return std::optional<FrameExtent>{FrameExtent{start, size}};
}
} // namespace

std::vector<std::string> ServerFramer::feed(std::string_view bytes)
{
    std::vector<std::string> bodies;
    if (failure_.has_value())
    {
        return bodies;
    }
    pending_.append(bytes);
    std::size_t consumed = 0;
    for (;;)
    {
        const auto frame = next_frame(std::string_view(pending_).substr(consumed));
        if (!frame.has_value())
        {
            failure_ = frame.error();
            pending_.clear();
            return bodies;
        }
        const std::optional<FrameExtent> extent = frame.value();
        if (!extent.has_value())
        {
            break;
        }
        const auto [start, size] = extent.value();
        bodies.push_back(pending_.substr(consumed + start, size));
        consumed += start + size;
    }
    pending_.erase(0, consumed);
    return bodies;
}

std::optional<application::ServerFailure> ServerFramer::failure() const noexcept
{
    return failure_;
}

std::string framed_message(std::string_view body)
{
    std::string frame = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    frame.append(body);
    return frame;
}
} // namespace nenenib::adapters::win32
