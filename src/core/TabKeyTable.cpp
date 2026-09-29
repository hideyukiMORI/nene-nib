#include "TabKeyTable.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
// Ctrl+W は通常モードでだけ閉じる。押し間違えて閉じると失うものが大きい（ADR 0056 の決定 10）。
[[nodiscard]] std::optional<TabCommand> control_w_command(EditMode mode) noexcept
{
    switch (mode)
    {
    case EditMode::ordinary:
        return TabCommand::close;
    case EditMode::vim:
        return std::nullopt;
    }
    std::unreachable();
}
} // namespace

std::optional<TabCommand> tab_command_for(TabKey key, EditMode mode) noexcept
{
    switch (key)
    {
    case TabKey::control_t:
        return TabCommand::open;
    case TabKey::control_tab:
        return TabCommand::next;
    case TabKey::control_shift_tab:
        return TabCommand::previous;
    case TabKey::control_f4:
        return TabCommand::close;
    case TabKey::control_w:
        return control_w_command(mode);
    }
    std::unreachable();
}
} // namespace nenenib::core
