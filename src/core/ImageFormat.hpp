#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nenenib::core
{
// 画像の形式（施主決定 D79・ADR 0155 の追補 48・#1381）。判定は拡張子でなくファイルの先頭のバイト列
// で、出すのは PNG・JPEG・GIF（最初の 1 コマ）・BMP だけ。ほか（SVG・WebP・AVIF など）は unknown
// で、adapter（段 2）は復号せずに unsupported を返す。値が増えたら switch が落ちる（CPP-002）。
enum class ImageFormat : std::uint8_t
{
    png,
    jpeg,
    gif,
    bmp,
    unknown
};

// image_format_of が見る先頭のバイト数。adapter はファイルの先頭からこれだけ（短いファイルは全部）
// を渡せばよい。
inline constexpr std::size_t image_format_probe_bytes = 18;

// 先頭のバイト列 head から形式を決める純関数。PNG は 8 バイトの署名、JPEG は FF D8 FF、GIF は
// "GIF87a" か "GIF89a"、BMP は "BM" と、14 バイト目からの情報の頭の大きさ（little endian の 4 バイ
// ト）が 12・40・52・56・64・108・124 のどれか。足りない・合わない列は unknown。
[[nodiscard]] ImageFormat image_format_of(std::string_view head) noexcept;
} // namespace nenenib::core
