#pragma once

#include <cstdint>

namespace nenenib::application
{
// 補完の面を確定しうる鍵（#1971・ADR 0190 の 2026-11-24 の追補）。打鍵で自動に開いた面では、Tab は
// 選んでいなければいちばん上の候補で確定し、Enter は使う人が選んだときだけ確定する（選んでいなけれ
// ば面を閉じて改行を通す）。手で開いた面はどちらも選んでいる候補で確定する。
enum class CompletionCommitKey : std::uint8_t
{
    enter,
    tab
};
} // namespace nenenib::application
