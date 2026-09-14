#pragma once

#include "DisplayText.hpp"
#include "Palette.hpp"

namespace nenenib::application
{
// UI が写すだけの表示値（ARC-011）。どちらのメンバーも検証済みの値なので公開 aggregate。
struct EditorFrame
{
    core::DisplayText text;
    core::Palette palette;
};
} // namespace nenenib::application
