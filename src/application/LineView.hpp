#pragma once

#include "ColorSpan.hpp"
#include "ComposedLine.hpp"
#include "DisplayLine.hpp"
#include "LineNumber.hpp"
#include "LineWindow.hpp"
#include "SelectionSpan.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::application
{
// 見えている 1 行の表示値（ARC-011）。UI はこれを写すだけで本文には触らない。
// matches は検索の当たりの面、current_match はそのうちキャレットを含む 1 つ（ADR 0037 の
// 決定 4）。桁は選択と同じ LineSpanEvaluation::span で作る（ADR 0095）。全角・Tab・CRLFでも
// 選択と同じ位置に出る。
// display は描画用の行（ADR 0040 の決定 2）。制御文字を `^X`、書式用文字を `<xxxx>` に置き換えた
// 文字列と桁の対応表で、UI は layout をこれで作る。selection / matches
// の桁は本文（text）の桁のまま。
struct LineView
{
    core::LineNumber number;
    std::string text;
    core::DisplayLine display;
    core::SelectionSpan selection;
    std::vector<core::SelectionSpan> matches;
    std::optional<core::SelectionSpan> current_match;
    std::optional<ComposedLine> composed{};
    std::int64_t screen_row = 0;
    std::size_t screen_rows = 1;
    // 巨大な行の窓（ADR 0139）。
    LineWindow window{};
    // この行の文字組みの原点の、本文の欄の左端からのずれ（物理画素・横の送りを引いた値）。
    // 折り返す設定の普通の行は 0、それ以外は窓の x と横の送りから決まる（ADR 0139 の決定 5）。
    float left = 0;
    // 構文の色の区間（ADR 0156 の決定 7）。桁は matches と同じ本文の桁で、昇順で重ならない。
    // 字句器を持たない言語・巨大な行の窓では空。IME の差し込み行（composed）は、差し込みの後ろの
    // 区間を差し込んだ code point の数だけずらしてあり、差し込みの範囲には区間が無い（#839）。
    std::vector<core::ColorSpan> colors{};
};

[[nodiscard]] inline std::string_view presented_text(const LineView &line) noexcept
{
    return line.composed.has_value() ? std::string_view(line.composed.value().text)
                                     : std::string_view(line.display.text);
}
} // namespace nenenib::application
