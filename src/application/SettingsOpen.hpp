#pragma once

#include "EditorSettings.hpp"

#include <cstddef>
#include <string>

namespace nenenib::application
{
// 開いている設定の面（controller が持つ・ADR 0170 の 2026-11-18 の追補 2）。opened は開いたときの
// 設定の写し（元に戻す先・閉じるときに書くかの比べ先）、query は探す欄の文字、focus は焦点の行の
// core::settings_rows の添字（絞っても同じ行を指す）。
struct SettingsOpen
{
    core::EditorSettings opened;
    std::string query;
    std::size_t focus;
};
} // namespace nenenib::application
