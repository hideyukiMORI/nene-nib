#include "TabKeyTable.hpp"

#include "EditorOperation.hpp"
#include "KeyChord.hpp"
#include "OperationBindings.hpp"
#include "OperationKey.hpp"

#include <array>
#include <optional>
#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] constexpr KeyChord chord_of(TabKey key) noexcept
{
    switch (key)
    {
    case TabKey::control_t:
        return KeyChord{true, false, OperationKey::t};
    case TabKey::control_tab:
        return KeyChord{true, false, OperationKey::tab};
    case TabKey::control_shift_tab:
        return KeyChord{true, true, OperationKey::tab};
    case TabKey::control_f4:
        return KeyChord{true, false, OperationKey::f4};
    case TabKey::control_w:
        return KeyChord{true, false, OperationKey::w};
    }
    std::unreachable();
}

// タブの操作 → 帯の操作の表（#1790 で関数の長さの上限のため switch から表へ）。載っていない操作は
// 帯の操作ではない。
using TabCommandRow = std::pair<EditorOperation, TabCommand>;
constexpr std::array<TabCommandRow, 4> tab_commands{{
    {EditorOperation::new_tab, TabCommand::open},
    {EditorOperation::recent_tab, TabCommand::next},
    {EditorOperation::recent_tab_back, TabCommand::previous},
    {EditorOperation::close_tab, TabCommand::close},
}};

[[nodiscard]] std::optional<TabCommand> tab_command_of(EditorOperation operation) noexcept
{
    for (const auto &[listed, command] : tab_commands)
    {
        if (listed == operation)
        {
            return command;
        }
    }
    return std::nullopt;
}
} // namespace

// どの鍵がどのモードで効くかは割り当ての表 operation_bindings の行が決める（ADR 0078 の決定 4）。
// Vim の Ctrl+W は表に行が無いので値なしで、窓が Vim の engine の `Ctrl-w` へ流す（ADR 0148 の
// 決定 7）。
std::optional<TabCommand> tab_command_for(TabKey key, EditMode mode) noexcept
{
    return operation_for(chord_of(key), mode).and_then(tab_command_of);
}
} // namespace nenenib::core
