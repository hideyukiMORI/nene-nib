#pragma once

#include "ProcessInstance.hpp"
#include "ProcessQueryFailure.hpp"

#include <cstdint>
#include <expected>

namespace nenenib::adapters::win32
{
// pid の process の作成時刻を答える関数。置き場は OpenProcess で、試験は替え玉で答える（#1634）。
using ProcessCreatedOf =
    std::expected<std::uint64_t, ProcessQueryFailure> (*)(std::uint32_t pid) noexcept;
// OpenProcess の 1 本の判定（ADR 0135 の決定 3）。process_alive も同じ判定を読む。
// 無い pid（ERROR_INVALID_PARAMETER）と終わった process は gone。
// 拒否とほかの失敗は denied。生きていれば作成時刻。
[[nodiscard]] std::expected<std::uint64_t, ProcessQueryFailure>
process_created(std::uint32_t pid) noexcept;
// このプロセスの印（pid と作成時刻）。ファイルにもレジストリにも触れない。
[[nodiscard]] ProcessInstance current_process_instance() noexcept;
} // namespace nenenib::adapters::win32
