#pragma once

#include <cstdint>

namespace nenenib::application
{
// 画像を読めなかった理由（施主決定 D79・ADR 0155 の追補 53・#1381・ARC-010 / CPP-005）。どれも画像
// は「[画像: alt]」の文字のままで、知らせは出さない。値が増えたら switch が落ちる（CPP-002）。
// - missing: ファイルが無い（フォルダだった・経路が見つからないを含む）。
// - unreadable: 開けない・読めない（拒否・共有違反・読みの失敗）。
// - too_large: 圧縮のファイルが core::image_file_byte_limit を超える、または元の画素の数が
//   core::image_pixel_limit を超える。
// - unsupported: 先頭のバイト列が core::ImageFormat::unknown、または OS に復号器が無い。
// - corrupt: 形式は分かるが復号できない（壊れている）。adapter の返した画素が ImagePixels の決まり
//   に合わないときも application はこれとして扱う。
// - insufficient_memory: 読み・復号・縮小で記憶が足りない。
enum class ImageFailure : std::uint8_t
{
    missing,
    unreadable,
    too_large,
    unsupported,
    corrupt,
    insufficient_memory
};
} // namespace nenenib::application
