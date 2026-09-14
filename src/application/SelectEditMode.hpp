#pragma once

#include "EditMode.hpp"

namespace nenenib::application
{
// ステータスバーのトグル。意図は「どちらを選んだか」（ADR 0008 の決定 4）。
struct SelectEditMode
{
    core::EditMode mode;
};
} // namespace nenenib::application
