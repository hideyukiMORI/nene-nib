#include "AutosaveCodec.hpp"

#include "AbsolutePath.hpp"
#include "TextLines.hpp"
#include "Utf8.hpp"

#include <array>
#include <charconv>
#include <format>
#include <optional>
#include <system_error>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::AutosaveFailure;
using Lines = std::vector<std::string_view>;
using Fields = std::array<std::string_view, 4>;

// 符号も空白も無い 10 進だけを受ける。
[[nodiscard]] std::optional<std::uint64_t> number_of(std::string_view text)
{
    std::uint64_t value = 0;
    const auto *const end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    if (text.empty() || error != std::errc{} || stop != end)
    {
        return std::nullopt;
    }
    return value;
}

// 1 始まりの数。0 と数でないものは値なし。
[[nodiscard]] std::optional<std::size_t> counted(std::string_view text)
{
    const auto value = number_of(text);
    if (!value.has_value() || value.value() == 0)
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(value.value());
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

// 空はパス無し（無題）。それ以外は絶対の FilePath でなければ壊れている。
[[nodiscard]] std::optional<std::optional<core::FilePath>> path_of(std::string_view text)
{
    if (text.empty())
    {
        return std::optional<core::FilePath>{};
    }
    auto path = core::FilePath::parse(text);
    if (!path || !rooted_path_text(text))
    {
        return std::nullopt;
    }
    return std::optional<core::FilePath>{std::move(path).value()};
}

// entry の値を 4 欄とパスに割る。パスは最後の欄で行末まで。
[[nodiscard]] std::optional<Fields> fields_of(std::string_view &line)
{
    Fields fields{};
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
    return fields;
}

[[nodiscard]] std::optional<AutosaveEntry> entry_of(std::string_view entry, std::string_view title)
{
    const auto fields = fields_of(entry);
    if (!fields.has_value())
    {
        return std::nullopt;
    }
    const auto id = number_of(fields.value()[0]);
    const auto line = counted(fields.value()[1]);
    const auto column = counted(fields.value()[2]);
    auto path = path_of(entry);
    auto shown = core::DisplayText::parse(title);
    if (!id.has_value() || !line.has_value() || !column.has_value() || !path.has_value() ||
        !shown || fields.value()[3] != autosave_body_name(application::AutosaveId{id.value()}))
    {
        return std::nullopt;
    }
    return AutosaveEntry{
        application::AutosaveId{id.value()}, std::move(path).value(), std::move(shown).value(),
        core::TextPosition{core::LineNumber{line.value()}, core::Column{column.value()}}};
}

[[nodiscard]] std::expected<std::vector<AutosaveEntry>, Failure> entries_of(const Lines &lines)
{
    if ((lines.size() - 1) % 2 != 0 || (lines.size() - 1) / 2 > maximum_autosave_entries)
    {
        return std::unexpected(Failure::unreadable);
    }
    std::vector<AutosaveEntry> entries;
    entries.reserve((lines.size() - 1) / 2);
    for (std::size_t index = 1; index < lines.size(); index += 2)
    {
        const auto entry = header_value(lines.at(index), "entry");
        const auto title = header_value(lines.at(index + 1), "title");
        auto decoded = entry.has_value() && title.has_value()
                           ? entry_of(entry.value(), title.value())
                           : std::nullopt;
        if (!decoded.has_value())
        {
            return std::unexpected(Failure::unreadable);
        }
        entries.push_back(std::move(decoded).value());
    }
    return entries;
}
} // namespace

std::string autosave_body_name(application::AutosaveId id)
{
    return std::format("{}.body", id.value);
}

std::expected<std::vector<AutosaveEntry>, Failure> decode_autosave_entries(std::string_view bytes)
{
    if (bytes.starts_with("\xEF\xBB\xBF"))
    {
        bytes.remove_prefix(3);
    }
    if (bytes.size() > maximum_autosave_list_bytes || !core::validate_utf8(bytes))
    {
        return std::unexpected(Failure::unreadable);
    }
    const Lines lines = text_lines(bytes);
    if (lines.empty() || lines.front() != "version=1")
    {
        return std::unexpected(Failure::unreadable);
    }
    return entries_of(lines);
}

std::expected<std::string, Failure>
encode_autosave_entries(const std::vector<AutosaveEntry> &entries)
{
    if (entries.size() > maximum_autosave_entries)
    {
        return std::unexpected(Failure::unwritable);
    }
    std::string bytes = "version=1\n";
    for (const AutosaveEntry &entry : entries)
    {
        const std::string_view path =
            entry.path.has_value() ? entry.path.value().text() : std::string_view{};
        bytes += std::format("entry={},{},{},{},{}\ntitle={}\n", entry.id.value,
                             entry.caret.line.value, entry.caret.column.value,
                             autosave_body_name(entry.id), path, entry.title.text());
        if (bytes.size() > maximum_autosave_list_bytes)
        {
            return std::unexpected(Failure::unwritable);
        }
    }
    return bytes;
}
} // namespace nenenib::adapters::win32
