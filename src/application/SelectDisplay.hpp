#pragma once

#include "DisplayForm.hpp"

namespace nenenib::application
{
// 表示形式の選択（ADR 0140 の決定 2・3・試作）。開いたときの問いの「はい」と :set display= が送る。
struct SelectDisplay
{
    core::DisplayForm display;
};
} // namespace nenenib::application
