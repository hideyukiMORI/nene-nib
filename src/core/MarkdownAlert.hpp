#pragma once

#include "PreviewAlert.hpp"
#include "PreviewBlock.hpp"

#include <cstddef>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// GitHub の alert の印の行（#1848・D103）。line は改行を除いた 1 行で、前後の空白（空白・Tab）を
// 除いて `[!NOTE]` `[!TIP]` `[!IMPORTANT]` `[!WARNING]` `[!CAUTION]` のどれかだけなら（大小を
// 区別しない）その種類。
[[nodiscard]] std::optional<PreviewAlert> markdown_alert_line(std::string_view line) noexcept;

// alert の引用の最初の段落 block の先頭の印（`[!NOTE]` の綴りのまま・続くのは文字の終わりか空白か
// 改行）を、種類の名前と改行に置き換え、名前に alert_title の装飾を先頭に足す。後ろの装飾は
// ずらした量だけ動かす。印が先頭に無い・印の中に装飾がある段落は変えずに nullopt を返す。変えたら
// ずらした量（新しい文字の長さ − 古い文字の長さ・画像の節の位置も同じだけ動かす）を返す。
[[nodiscard]] std::optional<std::ptrdiff_t> markdown_alert_titled(PreviewBlock &block,
                                                                  PreviewAlert alert);
} // namespace nenenib::core
