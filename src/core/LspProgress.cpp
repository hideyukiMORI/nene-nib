#include "LspProgress.hpp"

#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "LspJsonRead.hpp"
#include "NoticeLine.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace nenenib::core
{
namespace
{
constexpr std::uint64_t full_percentage = 100;

[[nodiscard]] std::optional<LspId> token_of(const JsonObject &params)
{
    const auto found = json_member(params, "token");
    if (!found.has_value())
    {
        return std::nullopt;
    }
    const auto &value = found.value().get().value;
    if (const auto *text = std::get_if<std::string>(&value))
    {
        return LspId{*text};
    }
    const auto *number = std::get_if<JsonNumber>(&value);
    if (number != nullptr && number->integer.has_value())
    {
        return LspId{number->integer.value()};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<LspProgressKind> kind_of(const JsonObject &value)
{
    const auto *kind = json_text_member(value, "kind");
    if (kind == nullptr)
    {
        return std::nullopt;
    }
    if (*kind == "begin")
    {
        return LspProgressKind::begin;
    }
    if (*kind == "report")
    {
        return LspProgressKind::report;
    }
    if (*kind == "end")
    {
        return LspProgressKind::end;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> text_member(const JsonObject &object, std::string_view key)
{
    const auto *text = json_text_member(object, key);
    return text != nullptr ? std::optional{*text} : std::nullopt;
}

void append_part(std::string &line, const std::optional<std::string> &part)
{
    if (!part.has_value() || part.value().empty())
    {
        return;
    }
    line += ' ';
    line += part.value();
}
} // namespace

std::optional<LspProgressValue> lsp_progress_of(const LspNotification &notification)
{
    if (notification.method != lsp_progress_method)
    {
        return std::nullopt;
    }
    const auto *params = json_object_of(notification.params);
    const auto token = params != nullptr ? token_of(*params) : std::nullopt;
    const auto *value = params != nullptr ? json_object_member(*params, "value") : nullptr;
    const auto kind = value != nullptr ? kind_of(*value) : std::nullopt;
    if (!token.has_value() || !kind.has_value())
    {
        return std::nullopt;
    }
    const auto percentage = json_count_member(*value, "percentage");
    return LspProgressValue{.token = token.value(),
                            .kind = kind.value(),
                            .title = text_member(*value, "title"),
                            .message = text_member(*value, "message"),
                            .percentage =
                                percentage.has_value()
                                    ? std::optional{std::min(percentage.value(), full_percentage)}
                                    : std::nullopt};
}

std::optional<DisplayText> lsp_progress_text(std::string_view language,
                                             const LspProgressValue &progress)
{
    std::string line(language);
    line += ':';
    const std::size_t head = line.size();
    append_part(line, progress.title);
    append_part(line, progress.message);
    if (progress.percentage.has_value())
    {
        line += ' ';
        line += std::to_string(progress.percentage.value());
        line += '%';
    }
    if (line.size() == head)
    {
        return std::nullopt;
    }
    return notice_line(line);
}
} // namespace nenenib::core
