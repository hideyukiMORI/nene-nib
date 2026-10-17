#pragma once

#include "FontFamilyFailure.hpp"

#include <expected>
#include <string>
#include <vector>

namespace nenenib::application
{
// OS の字体の family 名（`:set guifont=` の補完・施主決定 D58・#597）。製品は ui/win32 が描画と同じ
// DirectWrite の factory から読む（ADR 0114 の TextLayoutPort と同じ置き場）。読むのは補完が字体を
// 求めた最初の 1 回だけで、起動・開く・切り替え・保存・打鍵の道では呼ばない。並びと重複は
// core::FontFamilies が整える。
class FontFamilyPort
{
  public:
    FontFamilyPort() = default;
    virtual ~FontFamilyPort() = default;
    FontFamilyPort(const FontFamilyPort &) = delete;
    FontFamilyPort(FontFamilyPort &&) = delete;
    FontFamilyPort &operator=(const FontFamilyPort &) = delete;
    FontFamilyPort &operator=(FontFamilyPort &&) = delete;
    [[nodiscard]] virtual std::expected<std::vector<std::string>, FontFamilyFailure> list() = 0;
};
} // namespace nenenib::application
