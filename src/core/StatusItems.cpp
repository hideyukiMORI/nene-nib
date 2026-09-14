#include "StatusItems.hpp"

#include <string>

namespace nenenib::core
{
namespace
{
// 表示できる値だけを作る経路なので parse は必ず成功する。失敗は不変条件の破れであって
// 期待される失敗ではない（CPP-005 / ARC-010）。
[[nodiscard]] DisplayText fixed(const std::string &text)
{
    return DisplayText::parse(text).value();
}

[[nodiscard]] std::string caret_position(const TextPosition &caret)
{
    return "行 " + std::to_string(caret.line.value) + ", 桁 " + std::to_string(caret.column.value);
}
} // namespace

std::array<DisplayText, status_item_count> status_items_for(const TextPosition &caret,
                                                            LineEnding ending)
{
    return {fixed(caret_position(caret)), fixed("UTF-8"),
            fixed(std::string(line_ending_label(ending)))};
}
} // namespace nenenib::core
