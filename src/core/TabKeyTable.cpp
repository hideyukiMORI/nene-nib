#include "TabKeyTable.hpp"

#include "EditorOperation.hpp"
#include "KeyChord.hpp"
#include "OperationBindings.hpp"
#include "OperationKey.hpp"

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

[[nodiscard]] std::optional<TabCommand> tab_command_of(EditorOperation operation) noexcept
{
    switch (operation)
    {
    case EditorOperation::new_tab:
        return TabCommand::open;
    case EditorOperation::recent_tab:
        return TabCommand::next;
    case EditorOperation::recent_tab_back:
        return TabCommand::previous;
    case EditorOperation::close_tab:
        return TabCommand::close;
    case EditorOperation::open_file:
    case EditorOperation::save:
    case EditorOperation::save_as:
    case EditorOperation::list_files:
    case EditorOperation::list_operations:
    case EditorOperation::toggle_bookmark:
    case EditorOperation::undo:
    case EditorOperation::redo:
    case EditorOperation::font_larger:
    case EditorOperation::font_smaller:
    case EditorOperation::font_reset:
    case EditorOperation::toggle_mode:
    case EditorOperation::split_vertical:
    case EditorOperation::split_horizontal:
    case EditorOperation::next_pane:
    case EditorOperation::close_pane:
    case EditorOperation::toggle_preview:
    case EditorOperation::paste_plain:
    case EditorOperation::paste_html:
    case EditorOperation::toggle_explorer:
    case EditorOperation::choose_explorer_root:
        return std::nullopt;
    }
    std::unreachable();
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
