#pragma once

#include "DisplayForm.hpp"

namespace nenenib::application
{
// 現在の文書の表示形式を選ぶ（ADR 0140 の決定 2・3）。開いたときの問いの「はい」と Ex の
// `:set display=` が送る。設定には保存しない。
struct SelectDisplay
{
    core::DisplayForm display;
};
} // namespace nenenib::application
