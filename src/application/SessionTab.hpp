#pragma once

#include "FilePath.hpp"
#include "LineNumber.hpp"
#include "TextPosition.hpp"

#include <cstddef>

namespace nenenib::application
{
// 前回のタブの 1 本（ADR 0059 の決定 1）。パスのあるタブだけで、カーソルは行と桁、画面の位置は
// 先頭の行、recency は使った順の順位（0 がいちばん最近）。どの欄も単独で妥当な値なので公開
// aggregate（CPP-003）。
struct SessionTab
{
    core::FilePath path;
    core::TextPosition caret;
    core::LineNumber first_visible;
    std::size_t recency;
};
} // namespace nenenib::application
