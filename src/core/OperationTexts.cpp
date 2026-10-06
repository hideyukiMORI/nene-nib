#include "OperationTexts.hpp"

#include <cstddef>
#include <utility>

namespace nenenib::core
{
namespace
{
// EditorOperation の値か。値を足すとこの switch がコンパイルで落ち（CPP-002）、足した後は下の
// 数え上げが 1 つ増えて、文字の表に行が無ければ static_assert が落ちる。
[[nodiscard]] consteval bool is_operation(EditorOperation operation) noexcept
{
    switch (operation)
    {
    case EditorOperation::open_file:
    case EditorOperation::save:
    case EditorOperation::save_as:
    case EditorOperation::new_tab:
    case EditorOperation::close_tab:
    case EditorOperation::recent_tab:
    case EditorOperation::recent_tab_back:
    case EditorOperation::list_files:
    case EditorOperation::list_operations:
    case EditorOperation::toggle_bookmark:
    case EditorOperation::undo:
    case EditorOperation::redo:
    case EditorOperation::font_larger:
    case EditorOperation::font_smaller:
    case EditorOperation::font_reset:
    case EditorOperation::toggle_mode:
        return true;
    }
    return false;
}

// 値は 0 から連続して並ぶので、最初に値でなくなる所までが値の数。
[[nodiscard]] consteval std::size_t operation_count() noexcept
{
    std::size_t count = 0;
    while (is_operation(static_cast<EditorOperation>(count)))
    {
        ++count;
    }
    return count;
}

// 表の i 行目が i 番目の値を持つ（EditorOperation の順に 1 回ずつ）。
[[nodiscard]] consteval bool texts_in_order() noexcept
{
    for (std::size_t index = 0; index < operation_texts.size(); ++index)
    {
        if (std::to_underlying(operation_texts[index].operation) != index)
        {
            return false;
        }
    }
    return true;
}
} // namespace

static_assert(operation_texts.size() == operation_count(),
              "operation_texts has one row for every EditorOperation");
static_assert(texts_in_order(), "operation_texts follows the order of EditorOperation");

const OperationText &operation_text(EditorOperation operation) noexcept
{
    return operation_texts[std::to_underlying(operation)];
}
} // namespace nenenib::core
