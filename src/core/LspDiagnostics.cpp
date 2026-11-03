#include "LspDiagnostics.hpp"

#include "Diagnostic.hpp"
#include "DiagnosticSeverity.hpp"
#include "DiagnosticsCount.hpp"
#include "JsonNull.hpp"
#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "LspPosition.hpp"
#include "Offset.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
namespace
{
using Member = std::optional<std::reference_wrapper<const JsonValue>>;
using Version = std::optional<std::uint64_t>;

constexpr std::uint64_t lowest_severity = 1;
constexpr std::uint64_t highest_severity = 4;

[[nodiscard]] const JsonObject *object_of(const JsonValue &value) noexcept
{
    return std::get_if<JsonObject>(&value.value);
}

[[nodiscard]] const JsonObject *object_member(const JsonObject &object, std::string_view key)
{
    const Member found = json_member(object, key);
    return found.has_value() ? object_of(found.value().get()) : nullptr;
}

[[nodiscard]] const std::string *text_member(const JsonObject &object, std::string_view key)
{
    const Member found = json_member(object, key);
    return found.has_value() ? std::get_if<std::string>(&found.value().get().value) : nullptr;
}

// 0 以上の整数の欄（Position の line / character・severity・version）。
[[nodiscard]] std::optional<std::uint64_t> count_of(const JsonValue &value) noexcept
{
    const auto *number = std::get_if<JsonNumber>(&value.value);
    if (number == nullptr || !number->integer.has_value() || number->integer.value() < 0)
    {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(number->integer.value());
}

[[nodiscard]] std::optional<std::uint64_t> count_member(const JsonObject &object,
                                                        std::string_view key)
{
    const Member found = json_member(object, key);
    return found.has_value() ? count_of(found.value().get()) : std::nullopt;
}

[[nodiscard]] std::optional<LspPosition> position_member(const JsonObject &object,
                                                         std::string_view key)
{
    const JsonObject *position = object_member(object, key);
    if (position == nullptr)
    {
        return std::nullopt;
    }
    const std::optional<std::uint64_t> line = count_member(*position, "line");
    const std::optional<std::uint64_t> character = count_member(*position, "character");
    if (!line.has_value() || !character.has_value())
    {
        return std::nullopt;
    }
    return LspPosition{.line = static_cast<std::size_t>(line.value()),
                       .character = static_cast<std::size_t>(character.value())};
}

[[nodiscard]] std::optional<LspRange> range_member(const JsonObject &object)
{
    const JsonObject *range = object_member(object, "range");
    if (range == nullptr)
    {
        return std::nullopt;
    }
    const std::optional<LspPosition> start = position_member(*range, "start");
    const std::optional<LspPosition> end = position_member(*range, "end");
    if (!start.has_value() || !end.has_value())
    {
        return std::nullopt;
    }
    return LspRange{.start = start.value(), .end = end.value()};
}

// severity の欄（LSP 3.17 の 1〜4）。欄が無ければ error、それ以外の値は値なし（その件を捨てる）。
[[nodiscard]] std::optional<DiagnosticSeverity> severity_member(const JsonObject &object)
{
    const Member found = json_member(object, "severity");
    if (!found.has_value())
    {
        return DiagnosticSeverity::error;
    }
    const std::optional<std::uint64_t> value = count_of(found.value().get());
    if (!value.has_value() || value.value() < lowest_severity || value.value() > highest_severity)
    {
        return std::nullopt;
    }
    return static_cast<DiagnosticSeverity>(value.value() - lowest_severity);
}

// 最初の改行（LF か CR）の前まで。走査は自前で書く（STL の検索は core の外へシンボルを出す）。
[[nodiscard]] std::string first_line(std::string_view message)
{
    std::size_t end = 0;
    while (end < message.size() && message[end] != '\n' && message[end] != '\r')
    {
        ++end;
    }
    return std::string{message.substr(0, end)};
}

[[nodiscard]] std::optional<Diagnostic>
diagnostic_of(const JsonValue &value, const TextBuffer &text, LspPositionEncoding encoding)
{
    const JsonObject *object = object_of(value);
    if (object == nullptr)
    {
        return std::nullopt;
    }
    const std::string *message = text_member(*object, "message");
    const std::optional<LspRange> range = range_member(*object);
    const std::optional<DiagnosticSeverity> severity = severity_member(*object);
    if (message == nullptr || !range.has_value() || !severity.has_value())
    {
        return std::nullopt;
    }
    const std::optional<OffsetRange> placed = lsp_range_to_offsets(text, range.value(), encoding);
    if (!placed.has_value())
    {
        return std::nullopt;
    }
    return Diagnostic{
        .range = placed.value(), .severity = severity.value(), .message = first_line(*message)};
}

// version の欄。無いか null なら値なし、0 以上の整数なら値、それ以外は不正。
[[nodiscard]] std::expected<Version, LspDiagnosticsFailure> version_member(const JsonObject &params)
{
    const Member found = json_member(params, "version");
    if (!found.has_value() || std::holds_alternative<JsonNull>(found.value().get().value))
    {
        return Version{};
    }
    const Version version = count_of(found.value().get());
    if (!version.has_value())
    {
        return std::unexpected{LspDiagnosticsFailure::invalid_params};
    }
    return version;
}
} // namespace

std::optional<OffsetRange> lsp_range_to_offsets(const TextBuffer &text, const LspRange &range,
                                                LspPositionEncoding encoding)
{
    const Offset whole{text.size_bytes()};
    const Offset start = lsp_offset_of(text, range.start, encoding).value_or(whole);
    const Offset end = lsp_offset_of(text, range.end, encoding).value_or(whole);
    if (end < start)
    {
        return std::nullopt;
    }
    return OffsetRange{.begin = start, .end = end};
}

std::expected<PublishedDiagnostics, LspDiagnosticsFailure>
lsp_published_diagnostics(const LspNotification &notification, const TextBuffer &text,
                          LspPositionEncoding encoding)
{
    if (notification.method != lsp_publish_diagnostics_method)
    {
        return std::unexpected{LspDiagnosticsFailure::not_publish_diagnostics};
    }
    const JsonObject *params = object_of(notification.params);
    if (params == nullptr)
    {
        return std::unexpected{LspDiagnosticsFailure::invalid_params};
    }
    const std::string *uri = text_member(*params, "uri");
    const Member listed = json_member(*params, "diagnostics");
    const auto *entries =
        listed.has_value() ? std::get_if<JsonArray>(&listed.value().get().value) : nullptr;
    const std::expected<Version, LspDiagnosticsFailure> version = version_member(*params);
    if (uri == nullptr || entries == nullptr || !version.has_value())
    {
        return std::unexpected{LspDiagnosticsFailure::invalid_params};
    }
    PublishedDiagnostics published{.uri = *uri,
                                   .version = version.value(),
                                   .diagnostics = {},
                                   .count = DiagnosticsCount::complete};
    for (const JsonValue &entry : *entries)
    {
        std::optional<Diagnostic> diagnostic = diagnostic_of(entry, text, encoding);
        if (!diagnostic.has_value())
        {
            continue;
        }
        if (published.diagnostics.size() == lsp_diagnostics_limit)
        {
            published.count = DiagnosticsCount::truncated;
            break;
        }
        published.diagnostics.push_back(std::move(diagnostic).value());
    }
    return published;
}
} // namespace nenenib::core
