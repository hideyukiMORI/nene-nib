#include "StartPress.hpp"

#include "FilePath.hpp"

namespace nenenib::core
{
namespace
{
// 行の中身が同じか（種類と path）。「はじめる」の行は path を持たないので種類だけで決まる。
[[nodiscard]] bool same_entry(const StartEntry &left, const StartEntry &right) noexcept
{
    if (left.kind != right.kind || left.path.has_value() != right.path.has_value())
    {
        return false;
    }
    return !left.path.has_value() || left.path.value() == right.path.value();
}
} // namespace

std::optional<StartPress> start_pressed(std::span<const StartEntry> entries,
                                        std::optional<std::size_t> row)
{
    if (!row.has_value() || row.value() >= entries.size())
    {
        return std::nullopt;
    }
    return StartPress{row.value(), entries[row.value()]};
}

std::optional<std::size_t> start_released(const std::optional<StartPress> &pressed,
                                          std::span<const StartEntry> entries,
                                          std::optional<std::size_t> row) noexcept
{
    if (!pressed.has_value() || !row.has_value() || row.value() != pressed.value().index ||
        row.value() >= entries.size() || !same_entry(entries[row.value()], pressed.value().entry))
    {
        return std::nullopt;
    }
    return row;
}
} // namespace nenenib::core
