#pragma once

#include "ImageSize.hpp"

#include <cstdint>
#include <vector>

namespace nenenib::application
{
// 復号した画像の画素（施主決定 D79・ADR 0155 の追補 53・#1381）。段 2 の adapter が ImagePort の結
// 果として作り、application が PreviewImages に持ち、段 3 の ui が ID2D1Bitmap1 にして描く。決まり:
// - original はファイルの元の大きさ（画素）。プレビューの配置の上限（1 画素 = 1 DIP・
//   core::image_placement の natural）に使う。core::image_pixels_fit(original) が真。
// - size は bgra の大きさで、core::image_decoded_size(original) と等しい（長辺 2048 以下・縮めるだ
//   け）。
// - bgra は上の行から並べ、1 行は size.width * 4 バイト（行の間の詰め物なし）、1 画素は B G R A の
//   順の 4 バイトで、色は alpha を掛けた premultiplied（WIC の GUID_WICPixelFormat32bppPBGRA・
//   Direct2D の DXGI_FORMAT_B8G8R8A8_UNORM と D2D1_ALPHA_MODE_PREMULTIPLIED にそのまま渡せる）。大
//   きさは size.width * size.height * 4。
// - GIF は最初の 1 コマだけ。どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。画素は大きいの
//   で、作った後は std::shared_ptr<const ImagePixels> で共有し写さない（frame と device lost の作り
//   直しも同じ値を引く）。
struct ImagePixels
{
    core::ImageSize original;
    core::ImageSize size;
    std::vector<std::uint8_t> bgra;
};
} // namespace nenenib::application
