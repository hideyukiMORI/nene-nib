#pragma once

#include "LineNumber.hpp"
#include "SelectionSpan.hpp"

#include <optional>
#include <string>
#include <vector>

namespace nenenib::application
{
// 見えている 1 行の表示値（ARC-011）。UI はこれを写すだけで本文には触らない。
// matches は検索の当たりの面、current_match はそのうちキャレットを含む 1 つ（ADR 0037 の
// 決定 4）。桁は選択と同じ span_of 1 本で作るので、全角・Tab・CRLF でも選択と同じ位置に出る。
struct LineView
{
    core::LineNumber number;
    std::string text;
    core::SelectionSpan selection;
    std::vector<core::SelectionSpan> matches;
    std::optional<core::SelectionSpan> current_match;
};
} // namespace nenenib::application
