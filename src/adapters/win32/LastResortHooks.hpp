#pragma once

#include <functional>

namespace nenenib::adapters::win32
{
// 最後の砦の手続き（LastResortArming）が UI スレッドで呼ぶ 2 つの口（ADR 0137 の追補 #1625）。
// backup は未保存の退避を 1 回だけ回し、全部書けたら真（製品は controller の
// back_up_before_defect）。notice は退避の結果で 1 行知らせる（製品は OS の箱・試験は箱を出さない
// 替え玉）。どちらも空なら呼ばない。投げても手続きが受ける（退避できなかったと数える）。
// 文の UTF-16 への変換は置くときに済ませ、手続きの中で確保を増やさない。
struct LastResortHooks
{
    std::function<bool()> backup;
    std::function<void(bool backed_up)> notice;
};
} // namespace nenenib::adapters::win32
