#pragma once

namespace nenenib::adapters::win32
{
// 他の process の作成時刻を読めなかった訳（#1634）。gone は無い pid か終わった process で、
// denied は拒否とほかの失敗（生きている側に倒す・ADR 0135 の決定 3 と同じ向き）。
enum class ProcessQueryFailure
{
    gone,
    denied,
};
} // namespace nenenib::adapters::win32
