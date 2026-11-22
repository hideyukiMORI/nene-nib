#pragma once

#include <cstdint>

namespace nenenib::application
{
// 設定の「言語サーバー」の節の行の右の押す物（LSP 段 S4・#1945）。use は「使う」（見つかった候補の
// 絶対パスの 1 行を設定に書く）、remove は「外す」（その行を設定から消し、動いていれば止める）、
// none は押す物が無い（見つからない・探している）。どちらも Enter か押した要素と離した要素が
// 同じクリックで動き、ファイルへは面を閉じるときに 1 回書く（ADR 0170 の決定 6）。
enum class LanguageServerRowAction : std::uint8_t
{
    none,
    use,
    remove
};
} // namespace nenenib::application
