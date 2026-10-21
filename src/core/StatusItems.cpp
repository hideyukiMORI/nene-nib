#include "StatusItems.hpp"

#include "LanguageTable.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <expected>
#include <limits>
#include <span>
#include <string_view>

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

[[nodiscard]] DisplayText caret_position(const TextPosition &caret)
{
    constexpr std::string_view prefix = "行 ";
    constexpr std::string_view separator = ", 桁 ";
    std::array<char, prefix.size() + separator.size() + 2U * decimal_digits> storage{};
    auto *end = std::copy(prefix.begin(), prefix.end(), storage.data());
    // 最大桁数の範囲なので、fixedと同じvalue()で不変条件を確認する。
    end =
        decimal_end(std::span<char, decimal_digits>{end, decimal_digits}, caret.line.value).value();
    end = std::copy(separator.begin(), separator.end(), end);
    end = decimal_end(std::span<char, decimal_digits>{end, decimal_digits}, caret.column.value)
              .value();
    return fixed(std::string_view(storage.data(), static_cast<std::size_t>(end - storage.data())));
}
} // namespace

std::array<DisplayText, status_item_count> status_items_for(const TextPosition &caret,
                                                            TextEncoding encoding,
                                                            LineEnding ending, Language language)
{
    return {caret_position(caret), fixed(encoding_label(encoding)),
            fixed(line_ending_label(ending)), fixed(language_name(language))};
}
} // namespace nenenib::core
