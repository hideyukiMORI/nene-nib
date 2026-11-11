#pragma once

#include "ExplorerLayout.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// エクスプローラーの面の線の形（stroke style・丸い端と継ぎ目）と本文の左上の角の 2 つの形（角の外
// を塗る面と弧・path geometry）の保持（ADR 0162 の追補 #1390 の 6・レビュー V-1428 の L-1）。
// renderer が 1 つ持ち、ExplorerPainter が frame ごとに借りる。作り直すのは描画先の factory か角の
// 鍵（面の右端・上端・半径・境界の太さ）が変わったときだけ（幅のドラッグ・DPI）。書式の作り直しと
// 面を消したときに renderer が clear で捨てる。作れなければ値なし（線は既定の形・角は丸めない）。
class ExplorerShapes final
{
  public:
    // factory の stroke style。作れなければ nullptr（DrawLine は既定の形で引く）。
    [[nodiscard]] ID2D1StrokeStyle *stroke(ID2D1Factory &factory);
    // 角の形を鍵に合わせて用意する。偽なら角を丸めない（radius 0 か作る失敗）。
    [[nodiscard]] bool corner(ID2D1Factory &factory, const core::ExplorerLayout &layout);
    [[nodiscard]] ID2D1PathGeometry *outside() const noexcept;
    [[nodiscard]] ID2D1PathGeometry *arc() const noexcept;
    // 作った回数（stroke style と角の形の組・試験が保持を確かめる）。
    [[nodiscard]] std::size_t made() const noexcept;
    void clear() noexcept;

  private:
    [[nodiscard]] bool build_corner(ID2D1Factory &factory);

    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> stroke_;
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> outside_;
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> arc_;
    std::int32_t right_ = 0;
    std::int32_t top_ = 0;
    std::int32_t radius_ = 0;
    std::int32_t border_ = 0;
    bool stroke_tried_ = false;
    std::size_t made_ = 0;
};
} // namespace nenenib::ui::win32
