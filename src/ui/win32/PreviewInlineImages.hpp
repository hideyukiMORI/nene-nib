#pragma once

#include "MarkdownDocument.hpp"
#include "PreviewImages.hpp"
#include "PreviewInlineBox.hpp"
#include "PreviewInlineImage.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <optional>
#include <vector>

namespace nenenib::ui::win32
{
// プレビューの画像の置き場の唯一の写し（施主決定 D79・ADR 0155 の追補 62・#1439）。高さを測る所
// （PreviewBlockTops）・描く所（PreviewPainter）・位置を引く所（PreviewPane::hit_block）が同じ関数を
// 読み、同じ箱で同じ layout を引く。

// 要素 block の中で読めた画像（images->pixels_of が値を返す節）の箱。
// frame_dips は要素の layout の幅（DIP）で、大きさは
// core::image_placement(original, frame_dips)。images が null（画像の節が無い）なら空で、
// 何も確保しない。読む前・読んでいる間・失敗の節は入れない（「[画像: alt]」の文字のまま）。
// 入れ子の画像は、外側が箱になればその中の節を入れない（範囲が重ならない）。
[[nodiscard]] std::vector<PreviewInlineImage>
preview_inline_images(const core::MarkdownDocument &document,
                      const application::PreviewImages *images, std::size_t block,
                      std::uint32_t frame_dips);
// 物理画素の幅 → DIP（切り捨て）。
[[nodiscard]] std::uint32_t preview_dips_of(float pixels, std::uint32_t dpi) noexcept;
// 箱の辺（DIP）→ 物理画素（四捨五入・1 より小さくしない）。inline の物の大きさと描く矩形が読む。
[[nodiscard]] float preview_inline_extent(std::uint32_t dips, std::uint32_t dpi) noexcept;
// layout の中の箱の左上（layout の原点から・物理画素）。箱の下端は箱のある行の baseline に置く
// （inline の物の baseline は高さと同じ）。layout が答えなければ nullopt。
[[nodiscard]] std::optional<D2D1_POINT_2F>
preview_inline_origin(IDWriteTextLayout &layout, PreviewInlineBox box, std::uint32_t dpi);
} // namespace nenenib::ui::win32
