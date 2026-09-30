#include "SessionCodec.hpp"

#include "AbsolutePath.hpp"
#include "TextLines.hpp"
#include "Utf8.hpp"

#include <array>
#include <charconv>
#include <format>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::SessionFailure;
using application::Session;
using application::SessionTab;
using Lines = std::vector<std::string_view>;

// 符号も空白も無い 10 進だけを受ける。
[[nodiscard]] std::optional<std::size_t> number_of(std::string_view text)
{
    std::size_t value = 0;
    const auto *const end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    if (text.empty() || error != std::errc{} || stop != end)
    {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<std::string_view> header_value(std::string_view line,
                                                           std::string_view key)
{
    if (!line.starts_with(key) || line.substr(key.size(), 1) != "=")
    {
        return std::nullopt;
    }
    return line.substr(key.size() + 1);
}

// 1 始まりの数。0 と数でないものは値なし。
[[nodiscard]] std::optional<std::size_t> counted(std::string_view text)
{
    const auto value = number_of(text);
    if (!value.has_value() || value.value() == 0)
    {
        return std::nullopt;
    }
    return value;
}

// 先頭の 4 欄を `,` で切り、残りの全体をパスにする。
[[nodiscard]] std::optional<SessionTab> tab_of(std::string_view line)
{
    std::array<std::string_view, 4> fields{};
    for (auto &field : fields)
    {
        const auto separator = line.find(',');
        if (separator == std::string_view::npos)
        {
            return std::nullopt;
        }
        field = line.substr(0, separator);
        line.remove_prefix(separator + 1);
    }
    const auto row = counted(fields[0]);
    const auto column = counted(fields[1]);
    const auto first = counted(fields[2]);
    const auto recency = number_of(fields[3]);
    auto path = core::FilePath::parse(line);
    if (!row.has_value() || !column.has_value() || !first.has_value() || !recency.has_value() ||
        !path || !rooted_path_text(line))
    {
        return std::nullopt;
    }
    return SessionTab{
        std::move(path).value(),
        core::TextPosition{core::LineNumber{row.value()}, core::Column{column.value()}},
        core::LineNumber{first.value()}, recency.value()};
}

// 順位は 0 から数えて重複も飛びも無い（タブの数と同じ数の順列）。
[[nodiscard]] bool ranked(const std::vector<SessionTab> &tabs)
{
    std::vector<bool> seen(tabs.size(), false);
    for (const SessionTab &tab : tabs)
    {
        if (tab.recency >= tabs.size() || seen.at(tab.recency))
        {
            return false;
        }
        seen.at(tab.recency) = true;
    }
    return true;
}

[[nodiscard]] std::expected<std::vector<SessionTab>, Failure> tabs_of(const Lines &lines)
{
    if (lines.size() - 2 > maximum_session_tabs)
    {
        return std::unexpected(Failure::too_large);
    }
    std::vector<SessionTab> tabs;
    tabs.reserve(lines.size() - 2);
    for (std::size_t index = 2; index < lines.size(); ++index)
    {
        auto tab = tab_of(lines.at(index));
        if (!tab.has_value())
        {
            return std::unexpected(Failure::malformed);
        }
        tabs.push_back(std::move(tab).value());
    }
    if (!ranked(tabs))
    {
        return std::unexpected(Failure::malformed);
    }
    return tabs;
}

[[nodiscard]] std::expected<std::size_t, Failure> active_of(const Lines &lines)
{
    if (lines.size() < 2)
    {
        return std::unexpected(Failure::malformed);
    }
    const auto text = header_value(lines.at(1), "active");
    const auto active = number_of(text.value_or(std::string_view{}));
    if (!active.has_value())
    {
        return std::unexpected(Failure::malformed);
    }
    return active.value();
}
} // namespace

std::expected<Session, Failure> decode_session(std::string_view bytes)
{
    if (bytes.starts_with("\xEF\xBB\xBF"))
    {
        bytes.remove_prefix(3);
    }
    const Lines lines = text_lines(bytes);
    const auto version =
        header_value(lines.empty() ? std::string_view{} : lines.front(), "version");
    if (!version.has_value())
    {
        return std::unexpected(Failure::malformed);
    }
    if (version.value() != "1")
    {
        return std::unexpected(Failure::unsupported_version);
    }
    if (!core::validate_utf8(bytes))
    {
        return std::unexpected(Failure::malformed);
    }
    const auto active = active_of(lines);
    if (!active)
    {
        return std::unexpected(active.error());
    }
    auto tabs = tabs_of(lines);
    if (!tabs)
    {
        return std::unexpected(tabs.error());
    }
    const bool within =
        active.value() < tabs.value().size() || (tabs.value().empty() && active.value() == 0);
    if (!within)
    {
        return std::unexpected(Failure::malformed);
    }
    return Session{std::move(tabs).value(), active.value()};
}

std::expected<std::string, Failure> encode_session(const Session &session)
{
    if (session.tabs.size() > maximum_session_tabs)
    {
        return std::unexpected(Failure::too_large);
    }
    std::string bytes = std::format("version=1\nactive={}\n", session.active);
    for (const SessionTab &tab : session.tabs)
    {
        bytes += std::format("{},{},{},{},{}\n", tab.caret.line.value, tab.caret.column.value,
                             tab.first_visible.value, tab.recency, tab.path.text());
        if (bytes.size() > maximum_session_bytes)
        {
            return std::unexpected(Failure::too_large);
        }
    }
    return bytes;
}
} // namespace nenenib::adapters::win32
