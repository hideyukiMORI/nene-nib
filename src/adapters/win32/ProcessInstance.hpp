#pragma once

#include <cstdint>

namespace nenenib::adapters::win32
{
// 1 回の起動を名指す印（#1634・ADR 0137 の追補）。pid は OS が再利用するので、process の作成時刻
// （GetProcessTimes の creation time・100 ns 単位の 64 bit）と組にする。作成時刻は等しいかだけを
// 見る識別の値で、大小も経過も見ない（時計を読むのではない・ADR 0135 の注記）。
// 値だけを運ぶ公開 aggregate（CPP-003）。
struct ProcessInstance
{
    std::uint32_t pid = 0;
    std::uint64_t created = 0;
};
} // namespace nenenib::adapters::win32
