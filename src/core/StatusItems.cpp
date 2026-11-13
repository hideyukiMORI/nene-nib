#include "StatusItems.hpp"

#include "LanguageTable.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 表示できる値だけを作る経路なので parse は必ず成功する。失敗は不変条件の破れであって
// 期待される失敗ではない（CPP-005 / ARC-010）。
[[nodiscard]] DisplayText fixed(std::string_view text)
{
    return DisplayText::parse(text).value();
}

constexpr auto decimal_digits =
    static_cast<std::size_t>(std::numeric_limits<std::size_t>::digits10) + 1U;

[[nodiscard]] std::expected<char *, std::errc>
decimal_end(std::span<char, decimal_digits> destination, std::size_t value)
{
    const auto result =
        std::to_chars(destination.data(), destination.data() + destination.size(), value);
    if (result.ec != std::errc{})
    {
        return std::unexpected(result.ec);
    }
    return result.ptr;
}

// 置き場 {0} {1} のある文を、確保なしで固定長の buffer に埋める（1 打鍵ごとに通る道・ADR 0123 の
// 追補 2026-11-13）。文の表の文は短い（UiTexts の試験が caret_text_capacity を守る）。
constexpr std::size_t caret_text_capacity = 64;

[[nodiscard]] DisplayText caret_position(const TextPosition &caret, DisplayLanguage language)
{
    const std::string_view text = ui_text(UiMessage::status_line_column, language);
    std::array<char, caret_text_capacity + 2U * decimal_digits> storage{};
    char *end = storage.data();
    std::size_t index = 0;
    while (index < text.size())
    {
        const auto rest = text.substr(index);
        if (rest.starts_with("{0}") || rest.starts_with("{1}"))
        {
            const std::size_t value = rest[1] == '0' ? caret.line.value : caret.column.value;
            // 最大桁数の範囲なので、fixedと同じvalue()で不変条件を確認する。
            end = decimal_end(std::span<char, decimal_digits>{end, decimal_digits}, value).value();
            index += 3;
            continue;
        }
        // 文の字は caret_text_capacity までで切る（buffer の外へ書かない・表の文は収まる）。
        if (static_cast<std::size_t>(end - storage.data()) < caret_text_capacity)
        {
            *end = text[index];
            ++end;
        }
        ++index;
    }
    return fixed(std::string_view(storage.data(), static_cast<std::size_t>(end - storage.data())));
}
} // namespace

std::optional<DisplayText> diagnostic_count_item(const DiagnosticCounts &counts)
{
    if (!counted(counts))
    {
        return std::nullopt;
    }
    std::string shown;
    if (counts.errors > 0)
    {
        shown += "E" + std::to_string(counts.errors);
    }
    if (counts.warnings > 0)
    {
        shown += (shown.empty() ? "W" : " W") + std::to_string(counts.warnings);
    }
    switch (counts.completeness)
    {
    case DiagnosticsCount::complete:
        break;
    case DiagnosticsCount::truncated:
        shown += "+";
        break;
    }
    return fixed(shown);
}

DisplayText caret_status_item(const TextPosition &caret, DisplayLanguage language)
{
    return caret_position(caret, language);
}

std::array<DisplayText, status_item_count> status_items_for(DisplayText caret_item,
                                                            TextEncoding encoding,
                                                            LineEnding ending, Language language)
{
    return {std::move(caret_item), fixed(encoding_label(encoding)),
            fixed(line_ending_label(ending)), fixed(language_name(language))};
}
} // namespace nenenib::core
