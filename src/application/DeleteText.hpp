#pragma once

#include "DeleteDirection.hpp"
#include "DeleteUnit.hpp"

namespace nenenib::application
{
// Backspace / Delete と、通常モードの Ctrl+Backspace / Ctrl+Delete（unit が word）。選択がある
// ときは向きと大きさに関わらず選択を消す（ADR 0009 の追補 #1394）。
struct DeleteText
{
    core::DeleteDirection direction;
    core::DeleteUnit unit;
};
} // namespace nenenib::application
