#pragma once

#include "SettingsRowView.hpp"
#include "SettingsSection.hpp"
#include "SettingsSectionTab.hpp"
#include "SpeedSummary.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::application
{
// 設定の面の表示値（ADR 0170 の決定 5・2026-11-18 の追補 2・3・施主決定 D102）。開いている間だけ
// frame に載る。ui は読むだけで、項目の列・絞り込み・要約・組を計算し直さない。
// - tabs: 節の札（6 つ・宣言の順）。section は焦点の行の節（行が無ければ値なし）。
// - rows: 探す欄で絞った後の見えている行（表の順）。focused は rows の中の焦点（無ければ値なし）。
// - query: 探す欄の文字。
// - summary: 要約の 1 行と今の組（core::speed_summary）。
// - lsp_commands: 言語サーバーの命令の行（言語の名前と値・読むだけ）。
// - changed: 開いたときの写しと違うか（「元に戻す」が効くか）。
struct SettingsView
{
    std::array<SettingsSectionTab, 6> tabs;
    std::optional<core::SettingsSection> section;
    std::vector<SettingsRowView> rows;
    std::optional<std::size_t> focused;
    std::string query;
    core::SpeedSummary summary;
    std::vector<std::pair<std::string, std::string>> lsp_commands;
    bool changed;
};
} // namespace nenenib::application
