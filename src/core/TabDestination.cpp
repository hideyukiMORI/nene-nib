#include "TabDestination.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] std::optional<std::size_t> forward_destination(std::optional<std::size_t> count,
                                                             std::size_t active,
                                                             std::size_t tab_count) noexcept
{
    if (!count.has_value())
    {
        return (active + 1) % tab_count;
    }
    const std::size_t number = count.value();
    if (number == 0 || number > tab_count)
    {
        return std::nullopt;
    }
    return number - 1;
}

[[nodiscard]] std::optional<std::size_t> backward_destination(std::optional<std::size_t> count,
                                                              std::size_t active,
                                                              std::size_t tab_count) noexcept
{
    const std::size_t steps = count.value_or(1);
    if (steps == 0)
    {
        return std::nullopt;
    }
    return (active + tab_count - (steps % tab_count)) % tab_count;
}
} // namespace

std::optional<std::size_t> tab_destination(const TabJump &jump, std::size_t active,
                                           std::size_t tab_count) noexcept
{
    // 帯には少なくとも 1 本あり、今の位置はその中にある（EditorState の不変条件）。破れた入力は
    // 行き先なしとして返し、呼び出し元の範囲の外の扱い（何もしない）に任せる。
    if (active >= tab_count)
    {
        return std::nullopt;
    }
    switch (jump.direction)
    {
    case TabJumpDirection::forward:
        return forward_destination(jump.count, active, tab_count);
    case TabJumpDirection::backward:
        return backward_destination(jump.count, active, tab_count);
    }
    std::unreachable();
}
} // namespace nenenib::core
