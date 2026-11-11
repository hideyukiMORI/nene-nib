#include "ClipboardKeys.hpp"
#include "ClipboardOperation.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "OperationBindings.hpp"
#include "OperationShortcut.hpp"
#include "VimMode.hpp"
#include "VimSpecialKeys.hpp"
#include "WindowChecks.hpp"

#include <windows.h>

#include <array>
#include <optional>
#include <tuple>

namespace nenenib::tests::ui
{
namespace
{
using nenenib::application::ClipboardOperation;
using nenenib::core::EditMode;
using nenenib::core::VimMode;
using Clipboard = std::optional<ClipboardOperation>;

// 鍵（仮想キー・Ctrl・Shift）→ 通常モード・Vim の INSERT・Vim の NORMAL・Vim の VISUAL で送る
// クリップボードの操作。Ctrl+C / V / X と同じ規則（コピーはどこでも、貼り付けと切り取りは通常と
// INSERT だけ）。表に無い組み合わせは空で、今までの表へ流れる。
using ClipboardRow = std::tuple<WPARAM, bool, bool, Clipboard, Clipboard, Clipboard, Clipboard>;
constexpr std::array<ClipboardRow, 9> clipboard_rows{{
    {VK_INSERT, true, false, ClipboardOperation::copy, ClipboardOperation::copy,
     ClipboardOperation::copy, ClipboardOperation::copy},
    {VK_INSERT, false, true, ClipboardOperation::paste, ClipboardOperation::paste, std::nullopt,
     std::nullopt},
    {VK_DELETE, false, true, ClipboardOperation::cut, ClipboardOperation::cut, std::nullopt,
     std::nullopt},
    // 修飾なしの Insert と Delete・Ctrl+Shift の組み・Ctrl+Delete は今までの道のまま。
    {VK_INSERT, false, false, std::nullopt, std::nullopt, std::nullopt, std::nullopt},
    {VK_DELETE, false, false, std::nullopt, std::nullopt, std::nullopt, std::nullopt},
    {VK_INSERT, true, true, std::nullopt, std::nullopt, std::nullopt, std::nullopt},
    {VK_DELETE, true, false, std::nullopt, std::nullopt, std::nullopt, std::nullopt},
    {VK_DELETE, true, true, std::nullopt, std::nullopt, std::nullopt, std::nullopt},
    {VK_BACK, false, true, std::nullopt, std::nullopt, std::nullopt, std::nullopt},
}};

// 窓の press_key と同じ組み立て（モードの答えを写しへ渡す）。
Clipboard sent(const ClipboardRow &row, EditMode mode, VimMode vim)
{
    return nenenib::ui::win32::clipboard_key_for(
        std::get<0>(row), std::get<1>(row), std::get<2>(row),
        nenenib::ui::win32::takes_os_clipboard_keys(mode, vim));
}

void verify_clipboard_keys()
{
    for (const ClipboardRow &row : clipboard_rows)
    {
        expect(sent(row, EditMode::ordinary, VimMode::normal) == std::get<3>(row),
               "an old clipboard key in the ordinary mode sends its clipboard operation");
        expect(sent(row, EditMode::vim, VimMode::insert) == std::get<4>(row),
               "an old clipboard key in Vim INSERT follows Ctrl+C / V / X");
        expect(sent(row, EditMode::vim, VimMode::normal) == std::get<5>(row),
               "Vim NORMAL keeps no OS paste and no cut for the old clipboard keys");
        expect(sent(row, EditMode::vim, VimMode::visual) == std::get<6>(row),
               "Vim VISUAL keeps no OS paste and no cut for the old clipboard keys");
        expect(sent(row, EditMode::vim, VimMode::visual_line) == std::get<6>(row) &&
                   sent(row, EditMode::vim, VimMode::visual_block) == std::get<6>(row),
               "every Vim VISUAL mode treats the old clipboard keys alike");
    }
    // Vim の NORMAL の Shift+Delete は今までどおり Vim の鍵も送らない（#1325 のレビュー L2）。
    expect(!nenenib::ui::win32::vim_special_for(VK_DELETE, true).has_value(),
           "Shift+Delete still sends no Vim key in Vim NORMAL");
}

void verify_new_tab_and_redo_keys()
{
    using nenenib::core::EditorOperation;
    using nenenib::core::operation_for;
    using nenenib::ui::win32::operation_chord;
    const auto new_tab = operation_chord('N', true, false);
    expect(new_tab.has_value(), "Ctrl+N is an operation chord");
    if (new_tab.has_value())
    {
        expect(operation_for(new_tab.value(), EditMode::ordinary) == EditorOperation::new_tab,
               "Ctrl+N opens a new tab in the ordinary mode");
        expect(!operation_for(new_tab.value(), EditMode::vim).has_value(),
               "Ctrl+N runs no operation in the Vim mode");
    }
    const auto shifted_n = operation_chord('N', true, true);
    expect(!shifted_n.has_value() ||
               !operation_for(shifted_n.value(), EditMode::ordinary).has_value(),
           "Ctrl+Shift+N runs no operation");
    expect(!operation_chord('N', false, false).has_value(), "N alone is not an operation chord");
    const auto redo = operation_chord('Z', true, true);
    expect(redo.has_value(), "Ctrl+Shift+Z is an operation chord");
    if (redo.has_value())
    {
        expect(operation_for(redo.value(), EditMode::ordinary) == EditorOperation::redo,
               "Ctrl+Shift+Z redoes in the ordinary mode");
        expect(!operation_for(redo.value(), EditMode::vim).has_value(),
               "Ctrl+Shift+Z runs no operation in the Vim mode");
    }
}
} // namespace

// 通常モードの Windows の標準の鍵（施主決定 D82・#1395・ADR 0009 の追補）。窓は鍵を意図に写すだけ
// で、写しの表と関数を窓を作らずに確かめる。実機のクリップボードには触れない。
void verify_standard_keys()
{
    verify_clipboard_keys();
    verify_new_tab_and_redo_keys();
}
} // namespace nenenib::tests::ui
