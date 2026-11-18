#pragma once

#include "KeyChord.hpp"
#include "OperationKey.hpp"

#include <windows.h>

#include <array>
#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
// 仮想キー → 操作の鍵の写し（ADR 0078 の決定 2・13）。何が起きるかは core::operation_for が決める。
// 写すのは Ctrl つきの O S P Z Y + - 0 Space \、Ctrl+Shift+F4、修飾なしの F1 と F5、Ctrl なしの F6
// だけ。
// 主キーとテンキーの + - 0 は同じ値。`\` は JIS 配列の 2 つの物理キー（VK_OEM_5 の ¥ と
// VK_OEM_102 の ろ）を同じ値にする（ADR 0148 の決定 6）。Shift は押されているままを入れる（表に
// Shift つきの行がある）。Alt のときは呼ばない。T W D Tab と Shift なしの F4 は写さない（タブと
// ブックマークの鍵は TabShortcut と BookmarkKey の経路のまま）。
// F1 と F5（日時の挿入・ADR 0175）と F12（定義へ移動・ADR 0190 の決定 8）は修飾なしだけ、F6 は
// Ctrl なし（Shift はそのまま入れる）。
// Ctrl つきの M（プレビュー・ADR 0155）と Shift つきの V（文字だけの貼り付け・ADR 0157）も写す。
// 検索・置換・行へ移動の Ctrl つきの F H G と、Ctrl なしの F3（Shift はそのまま入れる）も写す
// （ADR 0174 の決定 1・Vim モードでは表に鍵の行が無いので今までどおり Vim の鍵へ流れる）。
// 通常モードの新しいタブの Ctrl+N も写す（D82・#1395・Vim では表に行が無く今までどおり）。
// ルートのフォルダを選ぶ Ctrl+Shift+R の R も写す（#1495・Shift なしは表に行が無く Vim の
// Ctrl-r のまま）。Shift なしの Ctrl+V（貼り付け・#1051）は表に行があるが写さない（今の貼り付けの
// 道のまま・Vim の矩形とエクスプローラーの焦点の振る舞いを変えない）。Ctrl+X / C / A と Delete も
// 同じく写さない。
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
    if (key == VK_F12 && !shift)
    {
        return core::KeyChord{false, false, core::OperationKey::f12};
    }
    return std::nullopt;
}

// Ctrl つきの英字 → 操作の鍵の表（CPP-012）。Shift は押されているままを入れる。V は Shift つき
// だけなので表の外。I は通常モードの補完の 2 つ目の鍵（施主決定 2026-11-18・Vim では表に行が無く
// 今までどおり）。
inline constexpr std::array<std::pair<WPARAM, core::OperationKey>, 13> operation_letters{
    {{'O', core::OperationKey::o},
     {'S', core::OperationKey::s},
     {'P', core::OperationKey::p},
     {'Z', core::OperationKey::z},
     {'Y', core::OperationKey::y},
     {'M', core::OperationKey::m},
     {'E', core::OperationKey::e},
     {'F', core::OperationKey::f},
     {'H', core::OperationKey::h},
     {'G', core::OperationKey::g},
     {'N', core::OperationKey::n},
     {'R', core::OperationKey::r},
     {'I', core::OperationKey::i}}};

[[nodiscard]] constexpr std::optional<core::OperationKey> operation_letter(WPARAM key) noexcept
{
    for (const auto &[letter, operation_key] : operation_letters)
    {
        if (key == letter)
        {
            return operation_key;
        }
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<core::KeyChord> operation_chord(WPARAM key, bool control,
                                                                   bool shift)
{
    if (key == VK_F1 || key == VK_F3 || key == VK_F5 || key == VK_F6 || key == VK_F12)
    {
        return function_chord(key, control, shift);
    }
    if (!control)
    {
        return std::nullopt;
    }
    const auto chord = [shift](core::OperationKey operation_key)
    { return std::optional{core::KeyChord{true, shift, operation_key}}; };
    if (const auto letter = operation_letter(key))
    {
        return chord(letter.value());
    }
    // OS の仮想キーは開いた集合（CPP-017）。
    switch (key)
    {
    case 'V':
        return shift ? chord(core::OperationKey::v) : std::nullopt;
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
    case VK_F4:
        return shift ? chord(core::OperationKey::f4) : std::nullopt;
    default:
        return std::nullopt;
    }
}
} // namespace nenenib::ui::win32
