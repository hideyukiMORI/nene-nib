#pragma once

#include "ImageSize.hpp"

#include <cstdint>

namespace nenenib::core
{
// 画像の大きさの純関数（ADR 0155 の追補 49・52・#1381）。どれも整数演算で、端数は四捨五入
// （std::lround を呼ばない・ARC-003）。縮めた辺は 1 より小さくしない。

// 元の画像を読んでよい画素の数か（幅と高さが 1 以上で、積が image_pixel_limit 以下）。
[[nodiscard]] bool image_pixels_fit(ImageSize original) noexcept;

// 復号して持つ大きさ。長辺が image_decoded_side_limit を超えれば長辺をそれに縮め、短辺を同じ比で縮
// める。超えなければ元のまま（拡大しない）。adapter（段 2）は WIC の縮小にこの大きさを渡し、
// application はこの大きさの画素だけを受け取る。
[[nodiscard]] ImageSize image_decoded_size(ImageSize original) noexcept;

// プレビューに描く大きさ（DIP）。original は元の画像の大きさ、frame_width は枠の本文の幅（DIP）。上
// 限は復号した大きさ image_decoded_size(original)（1 画素 = 1 DIP・DPI を見ない）で、画素を拡大しな
// い。その幅が frame_width 以下ならそのまま、広ければ幅を frame_width にして高さを元の比で縮める
// （長辺が 2048 を超える細長い画像は 1000×20000 → 102×2048）。丸めで 0 になる辺は 1。どれかが 0 な
// ら {0, 0}。
[[nodiscard]] ImageSize image_placement(ImageSize original, std::uint32_t frame_width) noexcept;
} // namespace nenenib::core
