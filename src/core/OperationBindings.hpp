#pragma once

#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "KeyChord.hpp"
#include "OperationBinding.hpp"
#include "OperationKey.hpp"
#include "OperationModes.hpp"

#include <array>
#include <optional>

namespace nenenib::core
{
// 操作の割り当ての表（ADR 0078 の決定 3）。鍵 → 操作と、一覧に見せる鍵の正本はこの 1 つ。
// 上から見て最初に当たる行が勝つ。Ctrl+W は通常モードでだけ閉じる（ADR 0056 の決定 10）。
// ブックマークは通常 Ctrl+D / Vim Ctrl+Shift+D（ADR 0063）。元に戻す・やり直すは通常モードだけ
// （Vim は u と Ctrl-r）。開くの Ctrl+O は通常モードだけで、Vim では Ctrl+Shift+O（Vim の Ctrl-o は
// ジャンプの戻り・D71・ADR 0152 の追補 #1037）。通常 / Vim の切り替えは Ctrl+Shift+Space
// で両モード（D61）。Ctrl+Space（Shift なし）は Vim INSERT の Ctrl-@ に残すので行を置かない。
// 「Shift を見ていなかった今の振る舞いを保つ行」は Shift なしの行のすぐ後ろに置く。見せる鍵は
// そのモードで最初に当たる行なので、一覧に見える鍵は Shift なしのまま（Vim の開くは
// Ctrl+Shift+O）。
inline constexpr std::array<OperationBinding, 36> operation_bindings{{
    {EditorOperation::open_file, OperationModes::ordinary, KeyChord{true, false, OperationKey::o}},
    // 通常モードでは Shift を見ていなかった今の振る舞いを保つ行で、Vim では開くの鍵（D71）。
    {EditorOperation::open_file, OperationModes::both, KeyChord{true, true, OperationKey::o}},
    {EditorOperation::save, OperationModes::both, KeyChord{true, false, OperationKey::s}},
    {EditorOperation::save_as, OperationModes::both, KeyChord{true, true, OperationKey::s}},
    {EditorOperation::new_tab, OperationModes::both, KeyChord{true, false, OperationKey::t}},
    // 枠の操作（ADR 0148 の決定 6）。Ctrl+Shift+F4 の枠を閉じる行は Ctrl+F4 のタブを閉じる行より
    // 前に置く（鍵は Shift の有無まで照合するので当たりは変わらないが、表の上から読む人が F4 の
    // 2 行を取り違えないように）。
    {EditorOperation::split_vertical, OperationModes::both,
     KeyChord{true, false, OperationKey::backslash}},
    {EditorOperation::split_horizontal, OperationModes::both,
     KeyChord{true, true, OperationKey::backslash}},
    {EditorOperation::next_pane, OperationModes::both, KeyChord{false, false, OperationKey::f6}},
    // Shift を見ていなかった今の振る舞いを保つ行。
    {EditorOperation::next_pane, OperationModes::both, KeyChord{false, true, OperationKey::f6}},
    {EditorOperation::close_pane, OperationModes::both, KeyChord{true, true, OperationKey::f4}},
    // Markdown のプレビュー（ADR 0155 の決定 3）。
    {EditorOperation::toggle_preview, OperationModes::both, KeyChord{true, true, OperationKey::m}},
    {EditorOperation::close_tab, OperationModes::ordinary, KeyChord{true, false, OperationKey::w}},
    {EditorOperation::close_tab, OperationModes::both, KeyChord{true, false, OperationKey::f4}},
    {EditorOperation::recent_tab, OperationModes::both, KeyChord{true, false, OperationKey::tab}},
    {EditorOperation::recent_tab_back, OperationModes::both,
     KeyChord{true, true, OperationKey::tab}},
    {EditorOperation::list_files, OperationModes::both, KeyChord{true, false, OperationKey::p}},
    // Shift を見ていなかった今の振る舞いを保つ行。
    {EditorOperation::list_files, OperationModes::both, KeyChord{true, true, OperationKey::p}},
    {EditorOperation::list_operations, OperationModes::both,
     KeyChord{false, false, OperationKey::f1}},
    {EditorOperation::toggle_bookmark, OperationModes::ordinary,
     KeyChord{true, false, OperationKey::d}},
    {EditorOperation::toggle_bookmark, OperationModes::vim, KeyChord{true, true, OperationKey::d}},
    {EditorOperation::undo, OperationModes::ordinary, KeyChord{true, false, OperationKey::z}},
    // Shift を見ていなかった今の振る舞いを保つ行。
    {EditorOperation::undo, OperationModes::ordinary, KeyChord{true, true, OperationKey::z}},
    {EditorOperation::redo, OperationModes::ordinary, KeyChord{true, false, OperationKey::y}},
    // Shift を見ていなかった今の振る舞いを保つ行。
    {EditorOperation::redo, OperationModes::ordinary, KeyChord{true, true, OperationKey::y}},
    {EditorOperation::font_larger, OperationModes::both, KeyChord{true, false, OperationKey::plus}},
    // Shift を見ていなかった今の振る舞いを保つ行。
    {EditorOperation::font_larger, OperationModes::both, KeyChord{true, true, OperationKey::plus}},
    {EditorOperation::font_smaller, OperationModes::both,
     KeyChord{true, false, OperationKey::minus}},
    // Shift を見ていなかった今の振る舞いを保つ行。
    {EditorOperation::font_smaller, OperationModes::both,
     KeyChord{true, true, OperationKey::minus}},
    {EditorOperation::font_reset, OperationModes::both, KeyChord{true, false, OperationKey::zero}},
    // Shift を見ていなかった今の振る舞いを保つ行。
    {EditorOperation::font_reset, OperationModes::both, KeyChord{true, true, OperationKey::zero}},
    {EditorOperation::toggle_mode, OperationModes::both, KeyChord{true, true, OperationKey::space}},
    // 貼り付け（ADR 0157 の決定 2・3）。Ctrl+Shift+V は文字だけ（Vim は INSERT だけで効く）。
    // 「HTML のまま貼り付け」は KeyChord に Alt が無いので鍵なしで、F1 の一覧からだけ。
    {EditorOperation::paste_plain, OperationModes::both, KeyChord{true, true, OperationKey::v}},
    {EditorOperation::paste_html, OperationModes::both, std::nullopt},
    // エクスプローラー（ADR 0162 の決定 7）。ルートのフォルダを選ぶのは鍵なしで F1 の一覧から。
    {EditorOperation::toggle_explorer, OperationModes::both, KeyChord{true, true, OperationKey::e}},
    {EditorOperation::choose_explorer_root, OperationModes::both, std::nullopt},
    // 日時の挿入（ADR 0175 の決定 3）。Vim の NORMAL / VISUAL では controller が知らせだけを出す。
    {EditorOperation::insert_date_time, OperationModes::both,
     KeyChord{false, false, OperationKey::f5}},
}};

// 鍵とモード → 操作。表を上から見て、鍵が同じでモードに効く最初の行。値なしは「割り当てが無い」。
[[nodiscard]] std::optional<EditorOperation> operation_for(KeyChord chord, EditMode mode) noexcept;

// 操作とモード → 一覧に見せる鍵。その操作の、モードに効く最初の行の鍵。鍵の無い行なら値なし。
[[nodiscard]] std::optional<KeyChord> shown_chord(EditorOperation operation,
                                                  EditMode mode) noexcept;

// 操作がそのモードで使えるか（モードに効く行が 1 つでもあるか）。
[[nodiscard]] bool operation_available(EditorOperation operation, EditMode mode) noexcept;
} // namespace nenenib::core
