#pragma once

#include <cstdint>

namespace nenenib::application
{
// プレビューの 1 つの画像のファイルの状態（ADR 0155 の追補 50・51・#1381）。loaded だけが画素を持
// ち、ほかは「[画像: alt]」の文字のまま。値が増えたら switch が落ちる（CPP-002）。
// - waiting: まだ頼んでいない（見える範囲に入ったら頼む）。
// - requested: 頼んで結果を待っている。
// - loaded: 読めた（画素を持つ）。
// - failed: 読めなかった（同じ文書の同じ経路の間は、再解析でも範囲の出入りでも頼み直さない）。
// - evicted: 読めたが、錨に近い画像を持つために見える範囲の中で捨てた。範囲の中にある間は（再解析の
//   後も）頼み直さず、範囲を出ると waiting に戻る（範囲の外で捨てたものは初めから waiting）。
enum class PreviewImageStatus : std::uint8_t
{
    waiting,
    requested,
    loaded,
    failed,
    evicted
};
} // namespace nenenib::application
