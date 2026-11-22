#pragma once

#include <cstdint>

namespace nenenib::application
{
// 設定の「言語サーバー」の節の 1 行の状態（LSP 段 S4・#1945・案 A）。running は設定に行があって
// 動いている・starting は起こしている途中（`initialize` の答えまで）・stopped は失敗が続いて止めた
// ・configured は設定に行があってまだ起こしていない・found は設定に行が無く PC で見つかった・
// missing は見つからない・searching は面を開いてからの最初の探しがまだ届いていない。ui は状態を
// 必ず語で書く（色だけで伝えない）。増えたら写し先の switch がコンパイルで落ちる（CPP-002）。
enum class LanguageServerRowState : std::uint8_t
{
    running,
    starting,
    stopped,
    configured,
    found,
    missing,
    searching
};
} // namespace nenenib::application
