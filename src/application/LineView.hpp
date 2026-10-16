#pragma once

#include "ComposedLine.hpp"
#include "DisplayLine.hpp"
#include "GridRowView.hpp"
#include "LineNumber.hpp"
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
    // 格子の画面行（ADR 0140・試作）。値があれば text / display はこの画面行の切れ端。
    std::optional<GridRowView> grid{};
};

[[nodiscard]] inline std::string_view presented_text(const LineView &line) noexcept
{
    return line.composed.has_value() ? std::string_view(line.composed.value().text)
                                     : std::string_view(line.display.text);
}
} // namespace nenenib::application
