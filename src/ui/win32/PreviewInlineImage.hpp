#pragma once

#include "ImagePixels.hpp"
#include "PreviewInlineBox.hpp"

#include <memory>

namespace nenenib::ui::win32
{
// 要素の中で読めた画像 1 枚（施主決定 D79・ADR 0155 の追補 62・#1439）。box は layout の中の箱、
// pixels は application の PreviewImages が持つ画素（読めた画像の pixels_of の写し）。layout の保持
// には box だけを入れ、pixels は描く間だけ借りる（ui は画素を持ち続けない）。どちらも単独で妥当な
// ので公開 aggregate（CPP-003）。
struct PreviewInlineImage
{
    PreviewInlineBox box;
    std::shared_ptr<const application::ImagePixels> pixels;
};
} // namespace nenenib::ui::win32
