#pragma once

#include <cstdint>

namespace nenenib::core
{
// 印刷の 1 ページの縦の寸法（ADR 0177 の決定 5）。どの値も印刷の面の論理画素（dpi は 1 インチの
// 論理画素の数）。header_height はヘッダ（ファイル名と「n / N」）の 1 行、row_height は本文の 1 行
// （紙の幅で折り返した後の画面行と同じ単位）の高さ。どの欄も単独で妥当な値なので公開 aggregate。
struct PrintSheet
{
    std::uint32_t height;
    std::uint32_t dpi;
    std::uint32_t header_height;
    std::uint32_t row_height;
};
} // namespace nenenib::core
