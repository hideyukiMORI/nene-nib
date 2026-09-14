#pragma once

#include "DeleteDirection.hpp"

namespace nenenib::application
{
// Backspace と Delete。選択があるときは向きに関わらず選択を消す。
struct DeleteText
{
    core::DeleteDirection direction;
};
} // namespace nenenib::application
