#pragma once

#include <cstddef>

namespace nenenib::application
{
// プレビューの画像を頼む「見える範囲」（施主決定 D79・ADR 0155 の追補 50・51・#1381）。要素の番号の
// [first_block, end_block) で、first_block は錨の要素。画像の近さは first_block からの要素の数の差
// で測る（範囲の外も同じ）。どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct PreviewImageWindow
{
    std::size_t first_block = 0;
    std::size_t end_block = 0;
};
} // namespace nenenib::application
