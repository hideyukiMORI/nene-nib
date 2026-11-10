#pragma once

#include "MinimapBlock.hpp"
#include "MinimapWindow.hpp"

#include <cstdint>
#include <vector>

namespace nenenib::application
{
// ミニマップの窓の行の塊（ADR 0171 の決定 3・4）。controller の MinimapCache が 1 つ持ち、編集の
// たびに書き換えて serial を進める（窓を作り直すときは別の値を作る）。renderer は frame で借りて
// 描くときだけ読み、指す先の同一性と serial で自分の bitmap と比べる。rows は window の行の順。
// どの欄も単独で妥当な値なので公開 aggregate。
// 前提（レビュー L8）: frame の MinimapFrame.rows は const の借りだが、MinimapCache は同じ値を
// 次の編集でその場で書き換える（rows も serial も）。frame は作ったらすぐ描くこと（いまは
// EditorController::frame() を呼ぶ所が描く直前の 1 か所）。frame を持ち越して後で描く道を作ると、
// frame の base と band は古く rows だけ新しい組になって帯を描き違える。その道を作るときは、
// 編集の前に rows を写す（copy-on-write）か、frame に serial を写して描く側で照合を足す。
struct MinimapRows
{
    core::MinimapWindow window;
    std::vector<core::MinimapRow> rows;
    std::uint64_t serial;
};
} // namespace nenenib::application
