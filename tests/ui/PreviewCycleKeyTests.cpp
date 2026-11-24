#include "DisplayLanguage.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "InputLinePrompt.hpp"
#include "InputLinePromptText.hpp"
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

// F12 の写し（ADR 0190 の決定 8・ADR 0191 の決定 4・#1954 のレビュー R4）。Shift+F12 は両モードで
// 参照の一覧、Shift なしは定義へ移動のまま、Ctrl つきは操作にならない。
void verify_reference_key()
{
    using nenenib::core::EditMode;
    using nenenib::core::EditorOperation;
    using nenenib::core::operation_for;
    using nenenib::ui::win32::operation_chord;
    const auto shifted = operation_chord(VK_F12, false, true);
    const auto plain = operation_chord(VK_F12, false, false);
    expect(shifted.has_value() &&
               operation_for(shifted.value(), EditMode::ordinary) ==
                   EditorOperation::list_references &&
               operation_for(shifted.value(), EditMode::vim) == EditorOperation::list_references,
           "Shift+F12 lists the references in both modes");
    expect(plain.has_value() && operation_for(plain.value(), EditMode::ordinary) ==
                                    EditorOperation::go_to_definition,
           "F12 without Shift still goes to the definition");
    const auto control = operation_chord(VK_F12, true, false);
    expect(!control.has_value() || !operation_for(control.value(), EditMode::ordinary).has_value(),
           "Ctrl+F12 runs no operation");
}

// F2 の写し（#2004・ADR 0194 の決定 7）。修飾なしの F2 は両モードで名前を変更、Shift と Ctrl つきは
// 操作にならない。入力行のプロンプトは「新しい名前: 」。
void verify_rename_key()
{
    using nenenib::core::EditMode;
    using nenenib::core::EditorOperation;
    using nenenib::core::operation_for;
    using nenenib::ui::win32::operation_chord;
    const auto plain = operation_chord(VK_F2, false, false);
    expect(plain.has_value() &&
               operation_for(plain.value(), EditMode::ordinary) == EditorOperation::rename_symbol &&
               operation_for(plain.value(), EditMode::vim) == EditorOperation::rename_symbol,
           "F2 renames in both modes");
    expect(!operation_chord(VK_F2, false, true).has_value() &&
               !operation_chord(VK_F2, true, false).has_value(),
           "Shift+F2 and Ctrl+F2 are not mapped");
    expect(nenenib::ui::win32::prompt_text(nenenib::core::InputLinePrompt::rename, {},
                                           nenenib::core::DisplayLanguage::japanese) ==
               "新しい名前: ",
           "the rename line shows its prompt");
}
} // namespace nenenib::tests::ui
