#pragma once

#include "DisplayCaret.hpp"
#include "TextCell.hpp"
#include "TextLayoutFailure.hpp"
#include "TextLayoutRequest.hpp"
#include "TextLocation.hpp"

#include <cstddef>
#include <expected>

namespace nenenib::application
{
// 文字組みの境界。製品は描画と同じDirectWrite資源から位置を返す（ADR 0114）。
class TextLayoutPort
{
  public:
    virtual ~TextLayoutPort() = default;
    [[nodiscard]] virtual std::expected<std::size_t, TextLayoutFailure>
    rows(const TextLayoutRequest &request) = 0;
    [[nodiscard]] virtual std::expected<core::TextLocation, TextLayoutFailure>
    locate(const TextLayoutRequest &request, core::DisplayCaret caret) = 0;
    [[nodiscard]] virtual std::expected<core::DisplayCaret, TextLayoutFailure>
    hit(const TextLayoutRequest &request, core::TextLocation point) = 0;
    // 格子の 1 升（ADR 0140 の決定 4・5）。request の text は見ず、書式と寸法だけを使う。
    [[nodiscard]] virtual std::expected<TextCell, TextLayoutFailure>
    cell(const TextLayoutRequest &request) = 0;
};
} // namespace nenenib::application
