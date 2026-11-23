#include "LspSignatureHelp.hpp"

#include "JsonNull.hpp"
#include "LspJsonRead.hpp"
#include "Offset.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
// 補助平面のコードポイントは UTF-16 で 2 単位。
constexpr char32_t last_basic_code_point = 0xFFFF;

// label の中の UTF-16 の位置 → バイトの位置（label の終わりで止める）。
[[nodiscard]] std::size_t utf16_to_bytes(std::string_view label, std::uint64_t units) noexcept
{
    std::size_t at = 0;
    std::uint64_t counted = 0;
    while (at < label.size() && counted < units)
    {
        counted += code_point_at(label, Offset{at}) > last_basic_code_point ? 2U : 1U;
        at = next_code_point(label, Offset{at}).value;
    }
    return at;
}

// `[start, end]` の配列の範囲（UTF-16）→ バイトの範囲。読めなければ値なし。
[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
offsets_range(std::string_view label, const JsonArray &pair)
{
    if (pair.size() != 2)
    {
        return std::nullopt;
    }
    const auto start = json_count_of(pair.front());
    const auto end = json_count_of(pair.back());
    if (!start.has_value() || !end.has_value() || end.value() < start.value())
    {
        return std::nullopt;
    }
    return std::pair{utf16_to_bytes(label, start.value()), utf16_to_bytes(label, end.value())};
}

// 1 つの引数の範囲（バイト）。`label` が文字列なら署名の label の from から後で最初の位置。
[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
parameter_range(std::string_view label, const JsonValue &parameter, std::size_t from)
{
    const JsonObject *object = json_object_of(parameter);
    const auto member = object != nullptr ? json_member(*object, "label") : std::nullopt;
    if (!member.has_value())
    {
        return std::nullopt;
    }
    const JsonValue &value = member.value().get();
    if (const auto *text = std::get_if<std::string>(&value.value); text != nullptr)
    {
        const auto found = text->empty() ? std::string_view::npos : label.find(*text, from);
        return found == std::string_view::npos
                   ? std::nullopt
                   : std::optional{std::pair{found, found + text->size()}};
    }
    const auto *pair = std::get_if<JsonArray>(&value.value);
    return pair != nullptr ? offsets_range(label, *pair) : std::nullopt;
}

// index 番目の引数の範囲（#1968 のレビュー c-R3）。文字列の label は前から順に探す: 探し始めは
// 署名の label の最初の `(` の後（無ければ先頭）で、2 つめからは前の引数の一致の終わり
// （`add(a, b)` の `a` が関数名に当たらない）。
[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
active_range(std::string_view label, const JsonArray &parameters, std::size_t index)
{
    const std::size_t open = label.find('(');
    std::size_t from = open == std::string_view::npos ? 0 : open + 1;
    for (std::size_t at = 0; at < index; ++at)
    {
        const auto passed = parameter_range(label, parameters.at(at), from);
        if (passed.has_value())
        {
            from = std::max(from, passed.value().second);
        }
    }
    return parameter_range(label, parameters.at(index), from);
}

// 今の引数の番号（c-R3）。署名の `activeParameter` が先で、無ければ全体の欄、どちらも無ければ 0。
// null（LSP 3.18 の「今の引数なし」）は値なしで強めない。引数の数の外は 0（LSP 3.17 の文）。
[[nodiscard]] std::optional<std::size_t> active_index(const JsonObject &signature,
                                                      const JsonObject &help, std::size_t count)
{
    auto member = json_member(signature, "activeParameter");
    if (!member.has_value())
    {
        member = json_member(help, "activeParameter");
    }
    std::uint64_t index = 0;
    if (member.has_value())
    {
        const JsonValue &value = member.value().get();
        if (std::holds_alternative<JsonNull>(value.value))
        {
            return std::nullopt;
        }
        index = json_count_of(value).value_or(0);
    }
    return index < count ? static_cast<std::size_t>(index) : 0;
}

// 1 つの署名を札の値に。label を上限で切り、強める範囲も中へ切る。
[[nodiscard]] std::optional<LspSignature> signature_of(const JsonObject &signature,
                                                       const JsonObject &help)
{
    const std::string *label = json_text_member(signature, "label");
    if (label == nullptr || label->empty())
    {
        return std::nullopt;
    }
    const auto parameters = json_member(signature, "parameters");
    const auto *list =
        parameters.has_value() ? std::get_if<JsonArray>(&parameters.value().get().value) : nullptr;
    const std::size_t count = list != nullptr ? list->size() : 0;
    const auto index = active_index(signature, help, count);
    const auto range =
        count > 0 && index.has_value() ? active_range(*label, *list, index.value()) : std::nullopt;
    std::string kept =
        label->substr(0, whole_code_points(std::string_view{*label}.substr(
                             0, std::min(label->size(), lsp_signature_label_limit))));
    const auto [begin, end] = range.value_or(std::pair<std::size_t, std::size_t>{0, 0});
    const std::size_t limit = kept.size();
    return LspSignature{.label = std::move(kept),
                        .active_begin = std::min(begin, limit),
                        .active_end = std::min(end, limit)};
}
} // namespace

std::expected<std::optional<LspSignature>, LspAnswerFailure>
lsp_signature_help(const JsonValue &result)
{
    if (std::holds_alternative<JsonNull>(result.value))
    {
        return std::optional<LspSignature>{};
    }
    const JsonObject *help = json_object_of(result);
    if (help == nullptr)
    {
        return std::unexpected{LspAnswerFailure::invalid_result};
    }
    const auto signatures = json_member(*help, "signatures");
    const auto *list =
        signatures.has_value() ? std::get_if<JsonArray>(&signatures.value().get().value) : nullptr;
    if (list == nullptr || list->empty())
    {
        return std::optional<LspSignature>{};
    }
    const std::uint64_t chosen = json_count_member(*help, "activeSignature").value_or(0);
    const JsonValue &signature =
        list->at(chosen < list->size() ? static_cast<std::size_t>(chosen) : 0);
    const JsonObject *object = json_object_of(signature);
    if (object == nullptr)
    {
        return std::optional<LspSignature>{};
    }
    return signature_of(*object, *help);
}
} // namespace nenenib::core
