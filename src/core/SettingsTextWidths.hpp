#pragma once

#include <array>
#include <cstdint>

namespace nenenib::core
{
// 設定の面の配置が要る文字の幅（物理画素・切り上げ・ui が描画と同じ書式で測る・#1836）。chips は
// 節の札の 6 つ（宣言の順）、presets は組の 3 つ、revert と close は下の帯のボタン。estimate は
// 要約の 1 行目（見込みと内訳）で、要約の箱に入り切らなければ配置が箱を 1 行ぶん高くする（文を
// 2 行に折る・合計の数字を「…」で失わない）。
struct SettingsTextWidths
{
    std::array<std::int32_t, 6> chips;
    std::array<std::int32_t, 3> presets;
    std::int32_t revert;
    std::int32_t close;
    std::int32_t estimate = 0;
};
} // namespace nenenib::core
