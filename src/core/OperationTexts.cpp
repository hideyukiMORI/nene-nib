#include "OperationTexts.hpp"

#include "OperationReadings.hpp"

#include <cstddef>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 値の数（最後の値から数える・UiMessage の ui_message_count と同じ形）。値を最後に足したら名指しも
// 移す。名指しが最後の値か・2 つの表の行の数と順は eng/conformance.py の CNF-016 が照合する（網羅の
// switch は 55 値で関数の長さの上限を超えた・#1790）。足した値に行が無ければ下の static_assert
// が落ちる。
[[nodiscard]] consteval std::size_t operation_count() noexcept
{
    return std::to_underlying(EditorOperation::quick_fix) + 1U;
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

// 読みの表の i 行目も i 番目の値を持つ（同じ形）。
[[nodiscard]] consteval bool readings_in_order() noexcept
{
    for (std::size_t index = 0; index < operation_readings.size(); ++index)
    {
        if (std::to_underlying(operation_readings[index].operation) != index ||
            operation_readings[index].reading.empty())
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
static_assert(operation_readings.size() == operation_count(),
              "operation_readings has one row for every EditorOperation");
static_assert(readings_in_order(),
              "operation_readings follows the order of EditorOperation with a reading in each row");

const OperationText &operation_text(EditorOperation operation) noexcept
{
    return operation_texts[std::to_underlying(operation)];
}

std::string_view operation_reading(EditorOperation operation) noexcept
{
    return operation_readings[std::to_underlying(operation)].reading;
}
} // namespace nenenib::core
