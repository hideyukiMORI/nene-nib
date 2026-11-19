#pragma once

#include "AutosaveId.hpp"
#include "TabSweepSide.hpp"

namespace nenenib::application
{
// 「ほかのタブを閉じる」「右のタブを閉じる」の頼み（ADR 0078 の追補 #1790）。keep は残すタブの
// 退避番号（帯の位置で名指ししない・閉じるたびに位置がずれる）。ui は帯の順に 1 つずつ × と同じ
// 閉じる道で閉じ、未保存のタブに当たるたびに「保存しますか」を聞き、取消で止める。値は単独で妥当
// なので公開 aggregate。
struct TabSweep
{
    AutosaveId keep;
    TabSweepSide side;
};
} // namespace nenenib::application
