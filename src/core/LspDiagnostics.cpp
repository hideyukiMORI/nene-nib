#include "LspDiagnostics.hpp"

#include "Diagnostic.hpp"
#include "DiagnosticSeverity.hpp"
#include "DiagnosticsCount.hpp"
#include "JsonNull.hpp"
#include "JsonValue.hpp"
#include "LspJsonRead.hpp"
#include "LspPosition.hpp"
#include "Offset.hpp"
#include "Utf8.hpp"

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

// severity の欄（LSP 3.17 の 1〜4）。欄が無ければ error、それ以外の値は値なし（その件を捨てる）。
[[nodiscard]] std::optional<DiagnosticSeverity> severity_member(const JsonObject &object)
{
    const Member found = json_member(object, "severity");
    if (!found.has_value())
    {
        return DiagnosticSeverity::error;
    }
    const std::optional<std::uint64_t> value = json_count_of(found.value().get());
    if (!value.has_value() || value.value() < lowest_severity || value.value() > highest_severity)
    {
        return std::nullopt;
    }
    return static_cast<DiagnosticSeverity>(value.value() - lowest_severity);
}

// 文の全部を上限のバイトまで（code point を割らない・#1939）。JSON の文字列は検証済みの UTF-8。
[[nodiscard]] std::string bounded_message(std::string_view message)
{
    if (message.size() <= lsp_diagnostic_message_limit)
    {
        return std::string{message};
    }
    std::size_t cut = lsp_diagnostic_message_limit;
    while (cut > 0 && !is_boundary(message, Offset{cut}))
    {
        --cut;
    }
    return std::string{message.substr(0, cut)};
}

// 欄 key の短い文字列（source）。無い・文字列でない・上限を越えるなら空。
[[nodiscard]] std::string label_member(const JsonObject &object, std::string_view key)
{
    const std::string *text = json_text_member(object, key);
    return text != nullptr && text->size() <= lsp_diagnostic_label_limit ? *text : std::string{};
}

// code の欄（LSP 3.17 は integer | string）。0 以上の整数は 10 進の文字列、文字列はそのまま
// （上限を越えたら空）、それ以外は空。
[[nodiscard]] std::string code_member(const JsonObject &object)
{
    const Member found = json_member(object, "code");
    if (!found.has_value())
    {
        return {};
    }
    if (const std::optional<std::uint64_t> number = json_count_of(found.value().get()))
    {
        return std::to_string(number.value());
    }
    return label_member(object, "code");
}

[[nodiscard]] std::optional<Diagnostic>
diagnostic_of(const JsonValue &value, const TextBuffer &text, LspPositionEncoding encoding)
{
    const JsonObject *object = json_object_of(value);
    if (object == nullptr)
    {
        return std::nullopt;
    }
    const std::string *message = json_text_member(*object, "message");
    const std::optional<LspRange> range = lsp_range_member(*object, "range");
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
    return Diagnostic{.range = placed.value(),
                      .severity = severity.value(),
                      .message = bounded_message(*message),
                      .source = label_member(*object, "source"),
                      .code = code_member(*object)};
}

// version の欄。無いか null なら値なし、0 以上の整数なら値、それ以外は不正。
[[nodiscard]] std::expected<Version, LspDiagnosticsFailure> version_member(const JsonObject &params)
{
    const Member found = json_member(params, "version");
    if (!found.has_value() || std::holds_alternative<JsonNull>(found.value().get().value))
    {
        return Version{};
    }
    const Version version = json_count_of(found.value().get());
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
    const JsonObject *params = json_object_of(notification.params);
    if (params == nullptr)
    {
        return std::unexpected{LspDiagnosticsFailure::invalid_params};
    }
    const std::string *uri = json_text_member(*params, "uri");
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
