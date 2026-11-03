#pragma once

#include <cstdint>

namespace nenenib::application
{
// 印刷の終わり方のうち失敗でないもの（ADR 0177 の決定 2・10）。cancelled は実装が印刷を始めずに
// 終えた場合で、controller は何も知らせない（ダイアログの取り消しは ui が意図を送らないので port
// までは来ない）。
enum class PrintOutcome : std::uint8_t
{
    printed,
    cancelled
};
} // namespace nenenib::application
