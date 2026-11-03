#pragma once

#include <cstdint>

namespace nenenib::application
{
// 印刷の終わり方のうち失敗でないもの（ADR 0177 の決定 2）。cancelled は印刷のダイアログで使う人が
// 取り消した場合で、controller は何も知らせない。
enum class PrintOutcome : std::uint8_t
{
    printed,
    cancelled
};
} // namespace nenenib::application
