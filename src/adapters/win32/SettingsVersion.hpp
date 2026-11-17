#pragma once

namespace nenenib::adapters::win32
{
enum class SettingsVersion
{
    v1,
    v2,
    v3,
    // ADR 0170 の決定 3 の鍵の集合（ADR 0114 の追補）。1.0.0 が読み書きする版で、鍵の集合は
    // `explorer_width` までで凍結した（施主決定 D91）。
    v4,
    // v4 の全部の鍵に 1.0.0 の後の鍵を足した版（ADR 0170 の 2026-11-16 の追補・#1086）。
    v5
};
} // namespace nenenib::adapters::win32
