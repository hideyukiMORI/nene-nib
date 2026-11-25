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
    v5,
    // v5 の全部の鍵にキャレットの動きの 2 つ（caret_motion・caret_motion_time）を足した版（ADR 0192
    // の 決定 1・#1933）。v5 は 1.1.0 が読み書きする版で、鍵の集合は閉じた。
    v6,
    // v6 の全部の鍵に不可視文字の 4 つ（list_eol・list_tab・list_wide_space・list_space）を足した版
    // （ADR 0193 の 2026-11-24 の追補 2・#1977）。v6 は 1.2.0
    // が読み書きする版で、鍵の集合は閉じた。
    v7
};
} // namespace nenenib::adapters::win32
