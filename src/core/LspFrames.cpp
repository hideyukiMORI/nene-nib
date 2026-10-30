#include "LspFrames.hpp"

#include "LspFrameFailure.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>
#include <system_error>

namespace nenenib::core
{
namespace
{
using Length = std::expected<std::size_t, LspFrameFailure>;
using Step = std::expected<void, LspFrameFailure>;

constexpr std::string_view header_end_mark = "\r\n\r\n";
constexpr std::string_view line_end_mark = "\r\n";
constexpr std::string_view length_name = "content-length";

[[nodiscard]] char ascii_lower(char byte) noexcept
{
    if (byte >= 'A' && byte <= 'Z')
    {
        return static_cast<char>(byte - 'A' + 'a');
    }
    return byte;
}

[[nodiscard]] bool is_length_name(std::string_view name) noexcept
{
    if (name.size() != length_name.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < name.size(); ++index)
    {
        if (ascii_lower(name[index]) != length_name[index])
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool is_blank(char byte) noexcept
{
    return byte == ' ' || byte == '\t';
}

// 前後の SP と HT を外す。find_first_not_of は memchr を core
// の外へ出すので歩いて数える（ARC-003）。
[[nodiscard]] std::string_view trimmed(std::string_view value) noexcept
{
    while (!value.empty() && is_blank(value.front()))
    {
        value.remove_prefix(1);
    }
    while (!value.empty() && is_blank(value.back()))
    {
        value.remove_suffix(1);
    }
    return value;
}

// `Content-Length` の値（空白を外した 10 進の数）。
[[nodiscard]] Length length_of(std::string_view value)
{
    const std::string_view digits = trimmed(value);
    const char *const last = digits.data() + digits.size();
    std::size_t length = 0;
    const auto result = std::from_chars(digits.data(), last, length);
    if (result.ec == std::errc::result_out_of_range)
    {
        return std::unexpected(LspFrameFailure::body_too_long);
    }
    if (digits.empty() || result.ec != std::errc{} || result.ptr != last)
    {
        return std::unexpected(LspFrameFailure::invalid_content_length);
    }
    if (length > lsp_body_limit)
    {
        return std::unexpected(LspFrameFailure::body_too_long);
    }
    return length;
}

// 見出しの 1 行を読む。`Content-Length` なら length に覚える（2 回目は誤り）。
[[nodiscard]] Step read_line(std::string_view line, std::optional<std::size_t> &length)
{
    // string_view::find(char) は memchr を core の外へ出すので、ranges::find で引く（ARC-003）。
    const auto separator = std::ranges::find(line, ':');
    if (separator == line.end() || separator == line.begin())
    {
        return std::unexpected(LspFrameFailure::malformed_header);
    }
    const auto colon = static_cast<std::size_t>(separator - line.begin());
    if (!is_length_name(line.substr(0, colon)))
    {
        return {};
    }
    if (length.has_value())
    {
        return std::unexpected(LspFrameFailure::invalid_content_length);
    }
    const Length value = length_of(line.substr(colon + 1));
    if (!value.has_value())
    {
        return std::unexpected(value.error());
    }
    length = value.value();
    return {};
}

// 空行の前までの見出しの行の列から本文の長さを読む。
[[nodiscard]] Length header_length(std::string_view header)
{
    std::optional<std::size_t> length;
    std::size_t at = 0;
    while (true)
    {
        const std::size_t end = header.find(line_end_mark, at);
        const Step step = read_line(header.substr(at, end - at), length);
        if (!step.has_value())
        {
            return std::unexpected(step.error());
        }
        if (end == std::string_view::npos)
        {
            break;
        }
        at = end + line_end_mark.size();
    }
    if (!length.has_value())
    {
        return std::unexpected(LspFrameFailure::missing_content_length);
    }
    return length.value();
}

// at から始まる 1 つの枠の本文の [begin, begin + length)。まだ揃っていなければ値なし。
using FrameBody = std::expected<std::optional<std::string_view>, LspFrameFailure>;

[[nodiscard]] FrameBody frame_at(std::string_view bytes, std::size_t at)
{
    const std::size_t header_end = bytes.find(header_end_mark, at);
    // 上限は空行を含めた見出しの長さの 1 本で決める。空行がまだ無ければ、届いた分よりあと少なくとも
    // 1 バイト要る。こうすると空行の途中のどこで読みが切れても結果が変わらない（#1165）。
    const std::size_t header_room = header_end == std::string_view::npos
                                        ? bytes.size() - at + 1U
                                        : header_end - at + header_end_mark.size();
    if (header_room > lsp_header_limit + header_end_mark.size())
    {
        return std::unexpected(LspFrameFailure::header_too_long);
    }
    if (header_end == std::string_view::npos)
    {
        return std::nullopt;
    }
    const std::size_t header_size = header_end - at;
    const Length length = header_length(bytes.substr(at, header_size));
    if (!length.has_value())
    {
        return std::unexpected(length.error());
    }
    const std::size_t begin = header_end + header_end_mark.size();
    if (bytes.size() - begin < length.value())
    {
        return std::nullopt;
    }
    return bytes.substr(begin, length.value());
}
} // namespace

std::expected<LspFrames, LspFrameFailure> lsp_frames(std::string_view bytes)
{
    LspFrames frames{.bodies = {}, .consumed = 0};
    while (frames.consumed < bytes.size())
    {
        const FrameBody body = frame_at(bytes, frames.consumed);
        if (!body.has_value())
        {
            return std::unexpected(body.error());
        }
        const std::optional<std::string_view> whole = body.value();
        if (!whole.has_value())
        {
            break;
        }
        const std::string_view found = whole.value();
        frames.bodies.push_back(found);
        frames.consumed = static_cast<std::size_t>(found.data() - bytes.data()) + found.size();
    }
    return frames;
}
} // namespace nenenib::core
