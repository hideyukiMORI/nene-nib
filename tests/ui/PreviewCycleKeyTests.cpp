#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "OperationBindings.hpp"
#include "OperationShortcut.hpp"
#include "WindowChecks.hpp"

#include <windows.h>

namespace nenenib::tests::ui
{
// Ctrl+Shift+M の振り分け（施主決定 D77・ADR 0155 の追補 #1354）。窓の鍵は通常モードでも Vim
// でも操作 toggle_preview の 1 つに当たり、ui は意図 TogglePreview を 1 つ送るだけ（巡回の次の
// 見せ方は application の cycled_preview_placement が決める）。鍵は増やさない: Ctrl+M（Shift なし）
// と Shift+M（Ctrl なし）はどの操作にも当たらない。
void verify_preview_cycle_key()
{
    using nenenib::core::EditMode;
    using nenenib::core::EditorOperation;
    using nenenib::core::operation_for;
    using nenenib::ui::win32::operation_chord;
    const auto chord = operation_chord('M', true, true);
    expect(chord.has_value(), "Ctrl+Shift+M is an operation chord");
    if (chord.has_value())
    {
        expect(operation_for(chord.value(), EditMode::ordinary) == EditorOperation::toggle_preview,
               "Ctrl+Shift+M runs toggle_preview in the ordinary mode");
        expect(operation_for(chord.value(), EditMode::vim) == EditorOperation::toggle_preview,
               "Ctrl+Shift+M runs toggle_preview in the Vim mode");
    }
    const auto control = operation_chord('M', true, false);
    expect(!control.has_value() || !operation_for(control.value(), EditMode::ordinary).has_value(),
           "Ctrl+M without Shift runs no operation");
    expect(!operation_chord('M', false, true).has_value(), "Shift+M is not an operation chord");
}
} // namespace nenenib::tests::ui
