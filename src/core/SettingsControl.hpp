#pragma once

#include <cstdint>

namespace nenenib::core
{
// 設定の面の行の右端の部品（施主決定 D102・#1836）。toggle はスイッチ、fixed は切れない機能の
// 「常にオン」の文字、choice は選ぶ欄（押すと次の値・左右の鍵で前後）、stepper は段の上下（− と
// ＋）、readonly は読むだけの欄（字体・一覧に OS の port が要る）。
enum class SettingsControl : std::uint8_t
{
    toggle,
    fixed,
    choice,
    stepper,
    readonly
};
} // namespace nenenib::core
