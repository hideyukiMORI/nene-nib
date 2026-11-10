#pragma once

#include <cstdint>

namespace nenenib::application
{
// プレビューの 1 つの画像のファイルの状態（ADR 0155 の追補 50・51・#1381）。loaded だけが画素を持
// ち、ほかは「[画像: alt]」の文字のまま。値が増えたら switch が落ちる（CPP-002）。
// - waiting: まだ頼んでいない（見える範囲に入ったら頼む）。
// - requested: 頼んで結果を待っている。
// - loaded: 読めた（画素を持つ）。
// - failed: 読めなかった（同じ文書の間は頼み直さない）。
// - evicted: 読めたが持つ量の上限で捨てた。見える範囲を出てから戻ったら頼み直す。
enum class PreviewImageStatus : std::uint8_t
{
    waiting,
    requested,
    loaded,
    failed,
    evicted
};
} // namespace nenenib::application
