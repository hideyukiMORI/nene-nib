#pragma once

#include "EditorSettings.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace nenenib::application
{
// 開いている設定の面（controller が持つ・ADR 0170 の 2026-11-18 の追補 2）。opened は開いたときの
// 設定の写し（元に戻す先・閉じるときに書くかの比べ先）、query は探す欄の文字、focus は焦点の行の
// 鍵（絞っても同じ行を指す）: core::settings_rows の添字か、その後ろに続く「言語サーバー」の節の
// 行の通し番号（表の順・畳んだ行も数える）と、最後の「ほかの N 言語を見る」（LSP 段 S4・#1945）。
// scroll は行の領域の送り（物理画素・ui が配置から 求めて ScrollSettings で置く・#1836）。
// servers_unfolded は「ほかの N 言語を見る」を押したか（面を閉じるまで開いたまま）。
struct SettingsOpen
{
    core::EditorSettings opened;
    std::string query;
    std::size_t focus;
    std::int32_t scroll = 0;
    bool servers_unfolded = false;
};
} // namespace nenenib::application
