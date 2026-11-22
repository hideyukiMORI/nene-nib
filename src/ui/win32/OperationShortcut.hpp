#pragma once

#include "KeyChord.hpp"
#include "OperationKey.hpp"

#include <windows.h>

#include <optional>

namespace nenenib::ui::win32
{
// 仮想キー → 操作の鍵の写し（ADR 0078 の決定 2・13）。何が起きるかは core::operation_for が決める。
// 写すのは Ctrl つきの O S P Z Y + - 0 Space \、Ctrl+Shift+F4、修飾なしの F1 と F5、Ctrl なしの F6
// だけ。
// 主キーとテンキーの + - 0 は同じ値。`\` は JIS 配列の 2 つの物理キー（VK_OEM_5 の ¥ と
// VK_OEM_102 の ろ）を同じ値にする（ADR 0148 の決定 6）。Shift は押されているままを入れる（表に
// Shift つきの行がある）。Alt のときは呼ばない。T W D Tab と Shift なしの F4 は写さない（タブと
// ブックマークの鍵は TabShortcut と BookmarkKey の経路のまま）。
// F1 と F5（日時の挿入・ADR 0175）は修飾なしだけ、F6 と F8（次・前の診断へ・#1939）と F12（定義へ
// 移動・ADR 0190 の決定 8・Shift+F12 は参照の一覧・#1947）は Ctrl なし（Shift はそのまま入れる）。
// Ctrl つきの M（プレビュー・ADR 0155）と Shift つきの V（文字だけの貼り付け・ADR 0157）も写す。
// 検索・置換・行へ移動の Ctrl つきの F H G と、Ctrl なしの F3（Shift はそのまま入れる）も写す
// （ADR 0174 の決定 1・Vim モードでは表に鍵の行が無いので今までどおり Vim の鍵へ流れる）。
// 通常モードの新しいタブの Ctrl+N も写す（D82・#1395・Vim では表に行が無く今までどおり）。
// ルートのフォルダを選ぶ Ctrl+Shift+R の R も写す（#1495・Shift なしは表に行が無く Vim の
// Ctrl-r のまま）。Shift なしの Ctrl+V（貼り付け・#1051）は表に行があるが写さない（今の貼り付けの
// 道のまま・Vim の矩形とエクスプローラーの焦点の振る舞いを変えない）。Ctrl+X / C / A と Delete も
// 同じく写さない。設定の面の Ctrl+,（#1836）も写す（Shift つきは表に行が無い）。
[[nodiscard]] inline std::optional<core::KeyChord> function_chord(WPARAM key, bool control,
                                                                  bool shift)
{
    if (control)
    {
        return std::nullopt;
    }
    if (key == VK_F6)
    {
        return core::KeyChord{false, shift, core::OperationKey::f6};
    }
    if (key == VK_F3)
    {
        return core::KeyChord{false, shift, core::OperationKey::f3};
    }
    if (key == VK_F1 && !shift)
    {
        return core::KeyChord{false, false, core::OperationKey::f1};
    }
    if (key == VK_F5 && !shift)
    {
        return core::KeyChord{false, false, core::OperationKey::f5};
    }
    // 定義へ移動と、Shift+F12 の参照の一覧（#1947）。
    if (key == VK_F12)
    {
        return core::KeyChord{false, shift, core::OperationKey::f12};
    }
    // 次・前の診断へ（#1939）。Shift はそのまま入れる（Shift+F8 は前へ）。
    if (key == VK_F8)
    {
        return core::KeyChord{false, shift, core::OperationKey::f8};
    }
    return std::nullopt;
}

// Ctrl つきの記号・数字・空白・F4 の鍵（operation_chord の続き・CPP-012 の関数の長さで分ける）。
[[nodiscard]] inline std::optional<core::KeyChord> symbol_chord(WPARAM key, bool shift)
{
    const auto chord = [shift](core::OperationKey operation_key)
    { return std::optional{core::KeyChord{true, shift, operation_key}}; };
    // OS の仮想キーは開いた集合（CPP-017）。
    switch (key)
    {
    case VK_OEM_PLUS:
    case VK_ADD:
        return chord(core::OperationKey::plus);
    case VK_OEM_MINUS:
    case VK_SUBTRACT:
        return chord(core::OperationKey::minus);
    case '0':
    case VK_NUMPAD0:
        return chord(core::OperationKey::zero);
    case VK_SPACE:
        return chord(core::OperationKey::space);
    case VK_OEM_5:
    case VK_OEM_102:
        return chord(core::OperationKey::backslash);
    case VK_OEM_COMMA:
        return chord(core::OperationKey::comma);
    case VK_F4:
        return shift ? chord(core::OperationKey::f4) : std::nullopt;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] inline std::optional<core::KeyChord> operation_chord(WPARAM key, bool control,
                                                                   bool shift)
{
    if (key == VK_F1 || key == VK_F3 || key == VK_F5 || key == VK_F6 || key == VK_F8 ||
        key == VK_F12)
    {
        return function_chord(key, control, shift);
    }
    if (!control)
    {
        return std::nullopt;
    }
    const auto chord = [shift](core::OperationKey operation_key)
    { return std::optional{core::KeyChord{true, shift, operation_key}}; };
    // OS の仮想キーは開いた集合（CPP-017）。
    switch (key)
    {
    case 'O':
        return chord(core::OperationKey::o);
    case 'S':
        return chord(core::OperationKey::s);
    case 'P':
        return chord(core::OperationKey::p);
    case 'Z':
        return chord(core::OperationKey::z);
    case 'Y':
        return chord(core::OperationKey::y);
    case 'M':
        return chord(core::OperationKey::m);
    case 'E':
        return chord(core::OperationKey::e);
    case 'V':
        return shift ? chord(core::OperationKey::v) : std::nullopt;
    case 'F':
        return chord(core::OperationKey::f);
    case 'H':
        return chord(core::OperationKey::h);
    case 'G':
        return chord(core::OperationKey::g);
    case 'N':
        return chord(core::OperationKey::n);
    // 通常モードの補完の 2 つ目の鍵 Ctrl+I（施主決定 D104・Vim では表に行が無い）。
    case 'I':
        return chord(core::OperationKey::i);
    case 'R':
        return chord(core::OperationKey::r);
    default:
        return symbol_chord(key, shift);
    }
}
} // namespace nenenib::ui::win32
