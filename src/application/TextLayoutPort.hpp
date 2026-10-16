#pragma once

#include "DisplayCaret.hpp"
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
    // 等幅の書式の 1 升の物理画素（ADR 0140 の決定 4・試作）。request の text は見ない。
    [[nodiscard]] virtual std::expected<float, TextLayoutFailure>
    cell(const TextLayoutRequest &request) = 0;
};
} // namespace nenenib::application
