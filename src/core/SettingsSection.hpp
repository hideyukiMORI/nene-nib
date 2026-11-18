#pragma once

#include <cstdint>

namespace nenenib::core
{
// 設定の面の節（ADR 0170 の決定 5・#1016 の段 1）。宣言の順が面の上からの順。
enum class SettingsSection : std::uint8_t
{
    display,
    text,
    editing,
    panes,
    features,
    language_server
};
} // namespace nenenib::core
