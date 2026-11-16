#pragma once

#include "DisplayText.hpp"
#include "ProjectSearchStatus.hpp"

#include <cstddef>

namespace nenenib::application
{
// 全文検索の面の進み具合の表示値（ADR 0167 の決定 5・ADR 0189 の D96）。status は今の状態、
// searched / found は要求からの累計のファイルの数、hits は面に出した当たりの数。line は面の先頭の
// 行と入力欄の右のどちらにも使える 1 行（ui_text_filled の文）。公開 aggregate（CPP-003）。
struct ProjectSearchView
{
    ProjectSearchStatus status;
    std::size_t searched;
    std::size_t found;
    std::size_t hits;
    core::DisplayText line;
};
} // namespace nenenib::application
