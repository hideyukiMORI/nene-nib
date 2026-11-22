#pragma once

#include "LanguageServerRowView.hpp"
#include "SettingsRowView.hpp"
#include "SettingsSection.hpp"
#include "SettingsSectionTab.hpp"
#include "SpeedSummary.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::application
{
// 設定の面の表示値（ADR 0170 の決定 5・2026-11-18 の追補 2・3・施主決定 D102）。開いている間だけ
// frame に載る。ui は読むだけで、項目の列・絞り込み・要約・組を計算し直さない。
// - tabs: 節の札（6 つ・宣言の順）。section は焦点の行の節（行が無ければ値なし）。
// - rows: 探す欄で絞った後の見えている設定の行（表の順）。
// - servers: 「言語サーバー」の節の見えている行（よく知られたサーバーの表の順・表に無い言語の
//   命令の行はその後ろ・LSP 段 S4・#1945）。servers_hidden は畳んだ行の数で、0 でなければ servers
//   の後ろに「ほかの N 言語を見る」の 1 行が続く。lsp_listed はこの節を出すか（探す欄で絞って
//   いる間は出さない）。
// - focused は見えている行の通しの位置（rows → servers → 畳みの 1 行の順・無ければ値なし）。
// - query: 探す欄の文字。
// - summary: 要約の 1 行と今の組（core::speed_summary）。
// - changed: 開いたときの写しと違うか（「元に戻す」が効くか）。
// - scroll: 行の領域の送り（物理画素・配置の入力・#1836）。
struct SettingsView
{
    std::array<SettingsSectionTab, 6> tabs;
    std::optional<core::SettingsSection> section;
    std::vector<SettingsRowView> rows;
    std::optional<std::size_t> focused;
    std::string query;
    core::SpeedSummary summary;
    std::vector<LanguageServerRowView> servers;
    bool changed;
    std::int32_t scroll = 0;
    bool lsp_listed = true;
    std::size_t servers_hidden = 0;
};
} // namespace nenenib::application
