#pragma once

#include <cstdint>

namespace nenenib::core
{
// 鍵がどこから来たか（ADR 0046 の決定 2）。typed は利用者の打鍵、replayed は controller が
// `.` と `@` の再生（VimReplay）で流している鍵。再生の中では `q` が効かず、録画中の
// マクロにも積まない（Vim の「レジスタ実行中は q は無効」と「録るのは打った鍵だけ」）。
enum class VimKeySource : std::uint8_t
{
    typed,
    replayed
};
} // namespace nenenib::core
