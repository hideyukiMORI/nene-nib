#pragma once

#include "ImageFailure.hpp"
#include "ImageFormat.hpp"
#include "ImagePixels.hpp"

#include <expected>
#include <memory>
#include <span>

namespace nenenib::adapters::win32
{
// 画像のファイルのバイト列を WIC で復号して縮める
// （施主決定 D79・ADR 0155 の追補 54・#1412）。
// - format は先頭のバイト列で決めた形式（core::image_format_of）で、
//   その形式の復号器だけを使う（拡張子は見ない）。GIF は最初の 1 コマ。
// - 元の大きさが core::image_pixel_limit を超えれば復号せずに too_large。
// - 画素は core::image_decoded_size に縮めた 32bppPBGRA で、ImagePixels の
//   大きさで 1 回だけ確保し、CopyPixels の stride は size.width * 4（詰め物なし）。
// - 復号器が無い（unknown を含む）→ unsupported、壊れている → corrupt、
//   記憶が足りない → insufficient_memory（std::bad_alloc は外へ出さない）。
// 形式が unknown なら COM に触れずに unsupported を返す。復号するときだけ、
// 呼んだスレッドで COM を MTA として初期化し、戻る前に解く（呼ぶのは
// Win32Worker の仕事の中だけ・ワーカーのほかの仕事は COM を使わない）。
// windowscodecs.dll は CoCreateInstance が最初の呼び出しで読む
// （exe は import しない・起動の道で読まない）。
// bytes は書き換えない（WIC の InitializeFromMemory が const でない BYTE * を
// 取るので、読める列を借りる）。
[[nodiscard]] std::expected<std::shared_ptr<const application::ImagePixels>,
                            application::ImageFailure>
decoded_image(std::span<char> bytes, core::ImageFormat format) noexcept;
} // namespace nenenib::adapters::win32
