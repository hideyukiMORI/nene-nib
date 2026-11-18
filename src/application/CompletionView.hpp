#pragma once

#include "CompletionRowView.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nenenib::application
{
// 補完の面の表示値（ADR 0169 の決定 4・ADR 0190 の決定 5）。面が見えている間だけ frame に載る。
// session は面の通し番号（ui は番号が変わったときに面の左端を決め直す）、rows は見えている行
// （最大 core::completion_face_rows・1 行以上）、selected は rows の中の選んでいる行、first は
// rows の先頭の候補の位置、total は今の接頭辞に当たる候補の数、detailed は当たっている候補のどれかに
// 補足があること（面の幅を打鍵ごとに変えない）。置き場は ui がキャレットの矩形から
// core::completion_face_layout で決める。
struct CompletionView
{
    std::uint64_t session;
    std::vector<CompletionRowView> rows;
    std::size_t selected;
    std::size_t first;
    std::size_t total;
    bool detailed;
};
} // namespace nenenib::application
