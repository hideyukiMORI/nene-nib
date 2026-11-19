#pragma once

#include <cstdint>

namespace nenenib::application
{
// リンクを OS の既定の道に渡せなかった理由（#1869 の段 L2）。refused は adapter の入口の守り
// （scheme が http / https / mailto でない・ASCII の URI の字でない）で断った、failed はシェルの
// 失敗。どちらも 1 行の知らせ。
enum class LinkOpenFailure : std::uint8_t
{
    refused,
    failed
};
} // namespace nenenib::application
