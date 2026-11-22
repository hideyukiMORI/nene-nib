#include "LspSymbols.hpp"

#include "JsonNull.hpp"
#include "LspJsonRead.hpp"
#include "LspRange.hpp"

#include <array>
#include <cstddef>
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
// 積みの 1 段（読む記号の値と親の名前）。
using Pending = std::pair<const JsonValue *, std::string>;

// `SymbolKind` の 1〜26 の名前（LSP 3.17）。
constexpr std::array<std::string_view, 26> kind_marks{
    {"file",     "module",        "namespace",   "package",     "class",     "method",
     "property", "field",         "constructor", "enum",        "interface", "function",
     "variable", "constant",      "string",      "number",      "boolean",   "array",
     "object",   "key",           "null",        "enum member", "struct",    "event",
     "operator", "type parameter"}};
constexpr std::string_view unknown_kind_mark = "symbol";

// 跳ぶ先の範囲。平らな答えは location.range、階層の答えは selectionRange（無ければ range）。
[[nodiscard]] std::optional<LspRange> symbol_range(const JsonObject &object)
{
    if (const JsonObject *location = json_object_member(object, "location"); location != nullptr)
    {
        return lsp_range_member(*location, "range");
    }
    const auto selection = lsp_range_member(object, "selectionRange");
    return selection.has_value() ? selection : lsp_range_member(object, "range");
}

// 記号 1 つ。親の名前は `containerName` が先、無ければ階層の親。
[[nodiscard]] std::optional<LspSymbol> symbol_of(const JsonObject &object,
                                                 const std::string &parent)
{
    const std::string *name = json_text_member(object, "name");
    const auto range = symbol_range(object);
    if (name == nullptr || !range.has_value())
    {
        return std::nullopt;
    }
    const std::string *container = json_text_member(object, "containerName");
    return LspSymbol{.name = *name,
                     .kind = json_count_member(object, "kind").value_or(0),
                     .parent = container != nullptr ? *container : parent,
                     .at = range.value()};
}

// 子を逆の順に積む（次に取り出すのが最初の子・文書の順を保つ）。
void push_children(std::vector<Pending> &pending, const JsonObject &object, const std::string &name)
{
    const auto children = json_member(object, "children");
    const auto *entries =
        children.has_value() ? std::get_if<JsonArray>(&children.value().get().value) : nullptr;
    if (entries == nullptr)
    {
        return;
    }
    for (auto entry = entries->rbegin(); entry != entries->rend(); ++entry)
    {
        pending.emplace_back(&*entry, name);
    }
}
} // namespace

std::expected<std::vector<LspSymbol>, LspAnswerFailure>
lsp_document_symbols(const JsonValue &result)
{
    std::vector<LspSymbol> symbols;
    if (std::holds_alternative<JsonNull>(result.value))
    {
        return symbols;
    }
    const auto *entries = std::get_if<JsonArray>(&result.value);
    if (entries == nullptr)
    {
        return std::unexpected{LspAnswerFailure::invalid_result};
    }
    std::vector<Pending> pending;
    for (auto entry = entries->rbegin(); entry != entries->rend(); ++entry)
    {
        pending.emplace_back(&*entry, std::string{});
    }
    while (!pending.empty() && symbols.size() < lsp_symbol_limit)
    {
        Pending next = std::move(pending.back());
        pending.pop_back();
        const JsonObject *object = json_object_of(*next.first);
        auto symbol = object != nullptr ? symbol_of(*object, next.second) : std::nullopt;
        if (!symbol.has_value())
        {
            continue;
        }
        push_children(pending, *object, symbol.value().name);
        symbols.push_back(std::move(symbol).value());
    }
    return symbols;
}

std::string_view lsp_symbol_kind_mark(std::uint64_t kind) noexcept
{
    if (kind == 0 || kind > kind_marks.size())
    {
        return unknown_kind_mark;
    }
    return kind_marks.at(static_cast<std::size_t>(kind - 1));
}
} // namespace nenenib::core
