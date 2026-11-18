#include "LspAnswers.hpp"

#include "JsonNull.hpp"
#include "LspCompletionItem.hpp"
#include "LspJsonRead.hpp"
#include "LspRange.hpp"
#include "LspSnippet.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
using Member = std::optional<std::reference_wrapper<const JsonValue>>;

constexpr std::uint64_t snippet_format = 2;
constexpr std::string_view fence = "```";

// object の鍵 key の文字列（無ければ fallback）。
[[nodiscard]] std::string text_or(const JsonObject &object, std::string_view key,
                                  std::string_view fallback)
{
    const std::string *found = json_text_member(object, key);
    return found != nullptr ? *found : std::string{fallback};
}

// `textEdit` の newText と範囲（`TextEdit` の range・`InsertReplaceEdit` の insert）。崩れていれば
// 値なし。
[[nodiscard]] std::optional<std::pair<std::string, LspRange>> text_edit(const JsonObject &item)
{
    const JsonObject *edit = json_object_member(item, "textEdit");
    const std::string *text = edit != nullptr ? json_text_member(*edit, "newText") : nullptr;
    if (text == nullptr)
    {
        return std::nullopt;
    }
    auto range = lsp_range_member(*edit, "range");
    if (!range.has_value())
    {
        range = lsp_range_member(*edit, "insert");
    }
    if (!range.has_value())
    {
        return std::nullopt;
    }
    return std::pair{*text, range.value()};
}

[[nodiscard]] std::optional<LspCompletionItem> completion_item(const JsonValue &value)
{
    const JsonObject *item = json_object_of(value);
    const std::string *label = item != nullptr ? json_text_member(*item, "label") : nullptr;
    if (label == nullptr)
    {
        return std::nullopt;
    }
    auto edit = text_edit(*item);
    std::string inserted =
        edit.has_value() ? std::move(edit.value().first) : text_or(*item, "insertText", *label);
    if (json_count_member(*item, "insertTextFormat") == std::optional{snippet_format})
    {
        inserted = lsp_snippet_text(inserted);
    }
    return LspCompletionItem{.label = *label,
                             .kind = json_count_member(*item, "kind"),
                             .detail = text_or(*item, "detail", ""),
                             .sort_text = text_or(*item, "sortText", *label),
                             .filter_text = text_or(*item, "filterText", *label),
                             .insert_text = std::move(inserted),
                             .edit_range = edit.has_value() ? std::optional{edit.value().second}
                                                            : std::nullopt};
}

[[nodiscard]] LspCompletionList completion_list(const JsonArray &entries, bool incomplete)
{
    LspCompletionList list{.incomplete = incomplete, .items = {}};
    for (const JsonValue &entry : entries)
    {
        // 切ったら手元で絞り直しても足りないので、出し直しを求める incomplete にする（レビュー
        // L1）。
        if (list.items.size() == lsp_completion_limit)
        {
            list.incomplete = true;
            break;
        }
        auto item = completion_item(entry);
        if (item.has_value())
        {
            list.items.push_back(std::move(item).value());
        }
    }
    return list;
}

// `MarkedString`・`MarkupContent` 1 つの平文。markdown なら囲みの行を外す印 markdown を立てる。
[[nodiscard]] std::optional<std::pair<std::string, bool>> marked_text(const JsonValue &value)
{
    if (const auto *text = std::get_if<std::string>(&value.value))
    {
        return std::pair{*text, true};
    }
    const JsonObject *object = json_object_of(value);
    const std::string *text = object != nullptr ? json_text_member(*object, "value") : nullptr;
    if (text == nullptr)
    {
        return std::nullopt;
    }
    const std::string *kind = json_text_member(*object, "kind");
    return std::pair{*text, kind != nullptr && *kind == "markdown"};
}

// 先頭の空白を飛ばして ``` で始まる行（markdown の囲み）。
[[nodiscard]] bool fence_line(std::string_view line) noexcept
{
    std::size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
    {
        ++start;
    }
    return line.substr(start, fence.size()) == fence;
}

// 平文を行に分けて積む（CR は落とし、markdown なら囲みの行を外す）。
void append_lines(std::vector<std::string_view> &lines, std::string_view text, bool markdown)
{
    std::size_t start = 0;
    while (start <= text.size())
    {
        std::size_t end = start;
        while (end < text.size() && text[end] != '\n')
        {
            ++end;
        }
        std::string_view line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        if (!markdown || !fence_line(line))
        {
            lines.push_back(line);
        }
        start = end + 1U;
    }
}

// 積んだ行 → 前後の空行を落として上限で切った平文（空なら値なし）。
[[nodiscard]] std::optional<std::string> joined(const std::vector<std::string_view> &lines)
{
    std::size_t first = 0;
    std::size_t last = lines.size();
    while (first < last && lines[first].empty())
    {
        ++first;
    }
    while (last > first && lines[last - 1U].empty())
    {
        --last;
    }
    if (first == last)
    {
        return std::nullopt;
    }
    last = first + std::min(last - first, lsp_hover_line_limit);
    std::string text;
    for (std::size_t index = first; index < last; ++index)
    {
        text += index == first ? "" : "\n";
        text += lines[index];
    }
    if (text.size() > lsp_hover_byte_limit)
    {
        text.resize(whole_code_points(std::string_view{text}.substr(0, lsp_hover_byte_limit)));
    }
    return text;
}

[[nodiscard]] std::optional<LspLocation> location_of(const JsonValue &value)
{
    const JsonObject *object = json_object_of(value);
    if (object == nullptr)
    {
        return std::nullopt;
    }
    const std::string *target = json_text_member(*object, "targetUri");
    if (target != nullptr)
    {
        auto range = lsp_range_member(*object, "targetSelectionRange");
        if (!range.has_value())
        {
            range = lsp_range_member(*object, "targetRange");
        }
        return range.has_value() ? std::optional{LspLocation{*target, range.value()}}
                                 : std::nullopt;
    }
    const std::string *uri = json_text_member(*object, "uri");
    const auto range = lsp_range_member(*object, "range");
    return uri != nullptr && range.has_value() ? std::optional{LspLocation{*uri, range.value()}}
                                               : std::nullopt;
}
} // namespace

std::expected<LspCompletionList, LspAnswerFailure> lsp_completion_items(const JsonValue &result)
{
    if (std::holds_alternative<JsonNull>(result.value))
    {
        return LspCompletionList{.incomplete = false, .items = {}};
    }
    if (const auto *entries = std::get_if<JsonArray>(&result.value))
    {
        return completion_list(*entries, false);
    }
    const JsonObject *list = json_object_of(result);
    const Member items = list != nullptr ? json_member(*list, "items") : std::nullopt;
    const auto *entries =
        items.has_value() ? std::get_if<JsonArray>(&items.value().get().value) : nullptr;
    if (entries == nullptr)
    {
        return std::unexpected{LspAnswerFailure::invalid_result};
    }
    const Member incomplete = json_member(*list, "isIncomplete");
    const auto *flag =
        incomplete.has_value() ? std::get_if<bool>(&incomplete.value().get().value) : nullptr;
    return completion_list(*entries, flag != nullptr && *flag);
}

std::optional<std::string> lsp_hover_text(const JsonValue &result)
{
    const JsonObject *hover = json_object_of(result);
    const Member contents = hover != nullptr ? json_member(*hover, "contents") : std::nullopt;
    if (!contents.has_value())
    {
        return std::nullopt;
    }
    std::vector<const JsonValue *> parts;
    if (const auto *entries = std::get_if<JsonArray>(&contents.value().get().value))
    {
        for (const JsonValue &entry : *entries)
        {
            parts.push_back(&entry);
        }
    }
    else
    {
        parts.push_back(&contents.value().get());
    }
    std::vector<std::string> texts;
    std::vector<std::string_view> lines;
    texts.reserve(parts.size());
    for (const JsonValue *part : parts)
    {
        auto marked = marked_text(*part);
        if (marked.has_value())
        {
            texts.push_back(std::move(marked.value().first));
            append_lines(lines, texts.back(), marked.value().second);
        }
    }
    return joined(lines);
}

std::expected<std::vector<LspLocation>, LspAnswerFailure> lsp_locations(const JsonValue &result)
{
    std::vector<LspLocation> locations;
    if (std::holds_alternative<JsonNull>(result.value))
    {
        return locations;
    }
    if (json_object_of(result) != nullptr)
    {
        auto location = location_of(result);
        if (location.has_value())
        {
            locations.push_back(std::move(location).value());
        }
        return locations;
    }
    const auto *entries = std::get_if<JsonArray>(&result.value);
    if (entries == nullptr)
    {
        return std::unexpected{LspAnswerFailure::invalid_result};
    }
    for (const JsonValue &entry : *entries)
    {
        auto location = location_of(entry);
        if (location.has_value())
        {
            locations.push_back(std::move(location).value());
        }
    }
    return locations;
}
} // namespace nenenib::core
