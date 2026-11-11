#pragma once

#include <cstddef>
#include <d2d1.h>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// エクスプローラーの面の線の形（stroke style・丸い端と継ぎ目）の保持（ADR 0162 の追補
// #1390 の 6・レビュー V-1428 の L-1）。renderer が 1 つ持ち、ExplorerPainter が frame
// ごとに借りる。作り直すのは描画先の factory が変わったときだけ。書式の作り直しと面を
// 消したときに renderer が clear で捨てる。作れなければ値なし（線は既定の形）。本文の帯の
// 左上の角の形は持たない（2026-11-11 夜に施主が丸みを外した・#1488）。
class ExplorerShapes final
{
  public:
    // factory の stroke style。作れなければ nullptr（DrawLine は既定の形で引く）。
    [[nodiscard]] ID2D1StrokeStyle *stroke(ID2D1Factory &factory);
    // 作った回数（stroke style・試験が保持を確かめる）。
    [[nodiscard]] std::size_t made() const noexcept;
    void clear() noexcept;

  private:
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> stroke_;
    bool stroke_tried_ = false;
    std::size_t made_ = 0;
};
} // namespace nenenib::ui::win32
