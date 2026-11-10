#pragma once

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// Markdown プレビューの画像の上限（施主決定 D79・ADR 0155 の追補 49・#1381）。値は NeNeFolio の ADR
// 0085 と同じ。上限を超えた画像は出さず、今の「[画像: alt]」の文字のまま。定数はここの 1 か所だけ
// で、adapters（段 2）と application と ui（段 3）が読む。

// 読む圧縮のファイルの大きさの上限（16 MiB）。超えたら adapter は復号せずに too_large を返す。
inline constexpr std::size_t image_file_byte_limit = std::size_t{16} * 1024 * 1024;

// 元の画像の画素の数の上限（6,400 万画素）。超えたら adapter は復号せずに too_large を返す。
inline constexpr std::uint64_t image_pixel_limit = 64'000'000;

// 復号して持つ画像の長辺の上限（2048 画素）。長辺がこれを超える画像は縦横比を保って縮めて持つ
// （image_decoded_size）。
inline constexpr std::uint32_t image_decoded_side_limit = 2048;

// 1 画素のバイト数（B G R A の 4 バイト・application の ImagePixels）。
inline constexpr std::size_t image_pixel_bytes = 4;

// 1 文書で出す画像の数の上限（本文の順で最初の 256 個・ADR 0155 の追補 50）。それより後ろの画像は頼
// まず「[画像: alt]」のまま。
inline constexpr std::size_t preview_image_limit = 256;

// application が持つ復号済みの画素の合計の上限（32 MiB）。超えたら読めた順の古いものから捨てる（ADR
// 0155 の追補 51）。長辺 2048 の画像 1 枚は 16 MiB なので、少なくとも 2 枚は持てる。
inline constexpr std::size_t preview_image_held_byte_limit = std::size_t{32} * 1024 * 1024;
} // namespace nenenib::core
