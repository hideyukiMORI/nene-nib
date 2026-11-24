#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::application
{
// 開いている選ぶ欄の一覧の表示値（設定の面の SettingsView に載る・#1926）。一覧は焦点の行の値の箱に
// 付く。choices は選択肢の表示の名前（列の順・core::settings_choice_texts）、current は今の値の位置
// （✓ を付ける・列に無い値なら値なし）、candidate は選ぼうとしている位置、top はいちばん上に見える
// 位置（送り・配置が切り詰める）、typed は頭文字の跳びのために続けて打った字。
struct SettingsChoiceView
{
    std::vector<std::string> choices;
    std::optional<std::size_t> current;
    std::size_t candidate;
    std::size_t top;
    std::string typed;
};
} // namespace nenenib::application
