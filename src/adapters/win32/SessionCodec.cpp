#include "SessionCodec.hpp"

#include "AbsolutePath.hpp"
#include "TextLines.hpp"
#include "Utf8.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <optional>
#include <span>
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

[[nodiscard]] bool supported_version(std::string_view version) noexcept
{
    return version == "1" || version == "2";
}

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

[[nodiscard]] std::optional<float> horizontal_of(std::string_view text)
{
    float value = 0;
    const auto *const end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    if (text.empty() || error != std::errc{} || stop != end || !std::isfinite(value) || value < 0)
    {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<core::CaretAffinity> affinity_of(std::string_view text)
{
    if (text == "leading")
    {
        return core::CaretAffinity::leading;
    }
    if (text == "trailing")
    {
        return core::CaretAffinity::trailing;
    }
    return std::nullopt;
}

[[nodiscard]] std::string_view affinity_text(core::CaretAffinity affinity)
{
    switch (affinity)
    {
    case core::CaretAffinity::leading:
        return "leading";
    case core::CaretAffinity::trailing:
        return "trailing";
    }
    std::unreachable();
}

[[nodiscard]] std::optional<SessionTab> base_tab(std::span<const std::string_view> fields,
                                                 std::string_view line)
{
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

[[nodiscard]] std::optional<SessionTab> wrapped_tab(SessionTab tab,
                                                    std::span<const std::string_view> fields)
{
    const auto row = number_of(fields[4]);
    const auto horizontal = horizontal_of(fields[5]);
    const auto affinity = affinity_of(fields[6]);
    if (!row.has_value() || !horizontal.has_value() || !affinity.has_value())
    {
        return std::nullopt;
    }
    tab.first_row = row.value();
    tab.horizontal = horizontal.value();
    tab.affinity = affinity.value();
    return tab;
}

// v1は4欄、v2は7欄。その残りの全体をパスにする。
[[nodiscard]] std::optional<SessionTab> tab_of(std::string_view line, bool wrapped)
{
    std::array<std::string_view, 7> fields{};
    for (auto &field : std::span(fields).first(wrapped ? 7U : 4U))
    {
        const auto separator = line.find(',');
        if (separator == std::string_view::npos)
        {
            return std::nullopt;
        }
        field = line.substr(0, separator);
        line.remove_prefix(separator + 1);
    }
    auto tab = base_tab(fields, line);
    if (!tab.has_value() || !wrapped)
    {
        return tab;
    }
    return wrapped_tab(std::move(tab).value(), fields);
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

[[nodiscard]] std::expected<std::vector<SessionTab>, Failure> tabs_of(const Lines &lines,
                                                                      bool wrapped)
{
    if (lines.size() - 2 > maximum_session_tabs)
    {
        return std::unexpected(Failure::too_large);
    }
    std::vector<SessionTab> tabs;
    tabs.reserve(lines.size() - 2);
    for (std::size_t index = 2; index < lines.size(); ++index)
    {
        auto tab = tab_of(lines.at(index), wrapped);
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
    if (!supported_version(version.value()))
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
    auto tabs = tabs_of(lines, version.value() == "2");
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
    std::string bytes = std::format("version=2\nactive={}\n", session.active);
    for (const SessionTab &tab : session.tabs)
    {
        bytes +=
            std::format("{},{},{},{},{},{},{},{}\n", tab.caret.line.value, tab.caret.column.value,
                        tab.first_visible.value, tab.recency, tab.first_row, tab.horizontal,
                        affinity_text(tab.affinity), tab.path.text());
        if (bytes.size() > maximum_session_bytes)
        {
            return std::unexpected(Failure::too_large);
        }
    }
    return bytes;
}
} // namespace nenenib::adapters::win32
