#pragma once

namespace nenenib::adapters::win32
{
enum class SettingsVersion
{
    v1,
    v2,
    v3,
    // ADR 0170 の決定 3 の鍵の集合（ADR 0114 の追補）。
    v4
};
} // namespace nenenib::adapters::win32
