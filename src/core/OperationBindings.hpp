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
// （Vim は u と Ctrl-r）。通常 / Vim の切り替えの鍵は未定（D10）で、一覧からだけ実行する。
inline constexpr std::array<OperationBinding, 18> operation_bindings{{
    {EditorOperation::open_file, OperationModes::both, KeyChord{true, false, OperationKey::o}},
    {EditorOperation::save, OperationModes::both, KeyChord{true, false, OperationKey::s}},
    {EditorOperation::save_as, OperationModes::both, KeyChord{true, true, OperationKey::s}},
    {EditorOperation::new_tab, OperationModes::both, KeyChord{true, false, OperationKey::t}},
    {EditorOperation::close_tab, OperationModes::ordinary, KeyChord{true, false, OperationKey::w}},
    {EditorOperation::close_tab, OperationModes::both, KeyChord{true, false, OperationKey::f4}},
    {EditorOperation::recent_tab, OperationModes::both, KeyChord{true, false, OperationKey::tab}},
    {EditorOperation::recent_tab_back, OperationModes::both,
     KeyChord{true, true, OperationKey::tab}},
    {EditorOperation::list_files, OperationModes::both, KeyChord{true, false, OperationKey::p}},
    {EditorOperation::list_operations, OperationModes::both,
     KeyChord{false, false, OperationKey::f1}},
    {EditorOperation::toggle_bookmark, OperationModes::ordinary,
     KeyChord{true, false, OperationKey::d}},
    {EditorOperation::toggle_bookmark, OperationModes::vim, KeyChord{true, true, OperationKey::d}},
    {EditorOperation::undo, OperationModes::ordinary, KeyChord{true, false, OperationKey::z}},
    {EditorOperation::redo, OperationModes::ordinary, KeyChord{true, false, OperationKey::y}},
    {EditorOperation::font_larger, OperationModes::both, KeyChord{true, false, OperationKey::plus}},
    {EditorOperation::font_smaller, OperationModes::both,
     KeyChord{true, false, OperationKey::minus}},
    {EditorOperation::font_reset, OperationModes::both, KeyChord{true, false, OperationKey::zero}},
    {EditorOperation::toggle_mode, OperationModes::both, std::nullopt},
}};

// 鍵とモード → 操作。表を上から見て、鍵が同じでモードに効く最初の行。値なしは「割り当てが無い」。
[[nodiscard]] std::optional<EditorOperation> operation_for(KeyChord chord, EditMode mode) noexcept;

// 操作とモード → 一覧に見せる鍵。その操作の、モードに効く最初の行の鍵。鍵の無い行なら値なし。
[[nodiscard]] std::optional<KeyChord> shown_chord(EditorOperation operation,
                                                  EditMode mode) noexcept;

// 操作がそのモードで使えるか（モードに効く行が 1 つでもあるか）。
[[nodiscard]] bool operation_available(EditorOperation operation, EditMode mode) noexcept;
} // namespace nenenib::core
