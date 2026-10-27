#pragma once

#include "MarkdownDocument.hpp"
#include "PreviewTextStyle.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <memory>
#include <string>

namespace nenenib::ui::win32
{
// プレビューの枠の絵（オフスクリーンの bitmap）を使い回してよいかの鍵（#903・ADR 0155 の追補）。
// 描く先（device context の同一性）・文書（指す先の同一性）・錨の要素・枠の幅と高さ（物理画素・
// 知らせの帯の高さはここに入る）・字体の設定（DPI を含む）・テーマの名前（brush の鍵と同じ）。
// どれかが違えば描き直す。互いに独立して妥当なので公開 aggregate（CPP-003）。
struct PreviewPictureKey
{
    const ID2D1RenderTarget *target;
    std::shared_ptr<const core::MarkdownDocument> document;
    std::size_t anchor;
    std::int32_t width;
    std::int32_t height;
    PreviewTextStyle style;
    std::string theme;
};

[[nodiscard]] inline bool operator==(const PreviewPictureKey &left, const PreviewPictureKey &right)
{
    return left.target == right.target && left.document == right.document &&
           left.anchor == right.anchor && left.width == right.width &&
           left.height == right.height && left.style == right.style && left.theme == right.theme;
}
} // namespace nenenib::ui::win32
