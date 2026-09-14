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

[[nodiscard]] std::string caret_position(std::size_t line, std::size_t column)
{
    return "行 " + std::to_string(line) + ", 桁 " + std::to_string(column);
}
} // namespace

std::array<DisplayText, status_item_count> status_items_for(std::size_t line, std::size_t column)
{
    return {fixed(caret_position(line, column)), fixed("UTF-8"), fixed("CRLF")};
}
} // namespace nenenib::core
