#pragma once

#include <cstddef>
#include <string>

namespace nenenib::application
{
// 開いている選ぶ欄の一覧（設定の面の SettingsOpen が持つ・#1926）。row は一覧を開いた
// 行の鍵（core::settings_rows の添字）、candidate は選ぼうとしている選択肢の位置、top は
// いちばん上に見える選択肢の位置（送り）、typed は頭文字の跳びのために続けて打った字。
// 選択肢の列は持たず、見せるたびに core::settings_choice_texts から作る（列の正本は 1 つ）。
struct SettingsChoiceOpen
{
    std::size_t row;
    std::size_t candidate;
    std::size_t top;
    std::string typed;
};
} // namespace nenenib::application
