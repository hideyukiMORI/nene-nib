#include "ClipboardKeys.hpp"
#include "ClipboardOperation.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "OperationBindings.hpp"
#include "OperationShortcut.hpp"
#include "TabShortcut.hpp"
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

// press_key の順（入力行 → タブの鍵 → 操作の表 → エクスプローラーの焦点 → clipboard_key_for →
// Ctrl の表 → Vim / 通常の表）の前提: 古いクリップボードの 3 つの鍵は、写しより前にあるタブの鍵と
// 操作の表のどのモードの行にも当たらない（当たると写しへ届かない・PR #1422 のレビュー L5）。
void verify_clipboard_keys_reach_their_turn()
{
    using nenenib::core::operation_for;
    using nenenib::ui::win32::operation_chord;
    using nenenib::ui::win32::tab_shortcut;
    using OldKey = std::tuple<WPARAM, bool, bool>;
    constexpr std::array<OldKey, 3> old_keys{
        {{VK_INSERT, true, false}, {VK_INSERT, false, true}, {VK_DELETE, false, true}}};
    for (const auto &[key, control, shifted] : old_keys)
    {
        const auto chord = operation_chord(key, control, shifted);
        expect(!chord.has_value() ||
                   (!operation_for(chord.value(), EditMode::ordinary).has_value() &&
                    !operation_for(chord.value(), EditMode::vim).has_value()),
               "no operation row catches an old clipboard key before clipboard_key_for");
        expect(!control || !tab_shortcut(key, shifted).has_value(),
               "no tab key catches an old clipboard key before clipboard_key_for");
    }
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
// ルートのフォルダを選ぶ Ctrl+Shift+R（施主決定 2026-11-11・#1495）。窓は Vim の鍵の前に操作の表を
// 引く（press_key）ので、表が Ctrl+Shift+R を両モードで拾えば Vim の NORMAL / INSERT / VISUAL と
// INSERT の `Ctrl-r` の待ちでも engine へ流れない。Shift なしの Ctrl+R は表・タブ・クリップボードの
// どれも拾わず、Vim の redo とレジスタの挿入のまま。エクスプローラーの Ctrl+Shift+E と同じ形。
void verify_folder_root_key()
{
    using nenenib::core::EditorOperation;
    using nenenib::core::operation_for;
    using nenenib::ui::win32::clipboard_key_for;
    using nenenib::ui::win32::operation_chord;
    using nenenib::ui::win32::tab_shortcut;
    const auto chosen = operation_chord('R', true, true);
    const auto explorer = operation_chord('E', true, true);
    expect(chosen.has_value() && explorer.has_value(), "Ctrl+Shift+R and E are operation chords");
    if (!chosen.has_value() || !explorer.has_value())
    {
        return;
    }
    expect(operation_for(chosen.value(), EditMode::ordinary) ==
                   EditorOperation::choose_explorer_root &&
               operation_for(chosen.value(), EditMode::vim) ==
                   EditorOperation::choose_explorer_root,
           "Ctrl+Shift+R chooses the explorer root in both modes before the Vim keys");
    expect(operation_for(explorer.value(), EditMode::vim) == EditorOperation::toggle_explorer &&
               !tab_shortcut('R', true).has_value() && !tab_shortcut('E', true).has_value(),
           "Ctrl+Shift+R takes the same path as Ctrl+Shift+E (no tab key catches either first)");
    const auto plain = operation_chord('R', true, false);
    bool passes =
        !plain.has_value() || (!operation_for(plain.value(), EditMode::ordinary).has_value() &&
                               !operation_for(plain.value(), EditMode::vim).has_value());
    passes = passes && !tab_shortcut('R', false).has_value();
    for (const bool os_keys : {true, false})
    {
        passes = passes && !clipboard_key_for('R', true, false, os_keys).has_value() &&
                 !clipboard_key_for('R', true, true, os_keys).has_value();
    }
    expect(passes, "Ctrl+R without Shift stays the Vim redo and register insertion");
}
} // namespace

// 通常モードの Windows の標準の鍵（施主決定 D82・#1395・ADR 0009 の追補）。窓は鍵を意図に写すだけ
// で、写しの表と関数を窓を作らずに確かめる。実機のクリップボードには触れない。
void verify_standard_keys()
{
    verify_clipboard_keys();
    verify_clipboard_keys_reach_their_turn();
    verify_new_tab_and_redo_keys();
    verify_folder_root_key();
}
} // namespace nenenib::tests::ui
