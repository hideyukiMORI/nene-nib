#include "StatusItems.hpp"

#include <string>
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

[[nodiscard]] std::string caret_position(const TextPosition &caret)
{
    return "行 " + std::to_string(caret.line.value) + ", 桁 " + std::to_string(caret.column.value);
}
} // namespace

std::array<DisplayText, status_item_count>
status_items_for(const TextPosition &caret, TextEncoding encoding, LineEnding ending)
{
    return {fixed(caret_position(caret)), fixed(encoding_label(encoding)),
            fixed(line_ending_label(ending))};
}
} // namespace nenenib::core
