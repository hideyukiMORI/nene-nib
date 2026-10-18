#pragma once

#include "DisplayCaret.hpp"
#include "TextCell.hpp"
#include "TextLayoutFailure.hpp"
#include "TextLayoutRequest.hpp"
#include "TextLocation.hpp"
#include "TextRow.hpp"

#include <cstddef>
#include <expected>
#include <variant>

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
    // 画面行の問い（ADR 0144 の決定 2・#664）。target が Column ならその桁の先頭側（次の段の最初の
    // 文字の前は次の段・段の末尾にぶら下がる空白はその段）を含む画面行、std::size_t ならその番目
    // （範囲の外は最後）の画面行。段の番号と範囲を同じ 1 つの layout の行の metrics から答える。
    [[nodiscard]] virtual std::expected<TextRow, TextLayoutFailure>
    row(const TextLayoutRequest &request, std::variant<core::Column, std::size_t> target) = 0;
    // 格子の 1 升（ADR 0140 の決定 4・5）。request の text は見ず、書式と寸法だけを使う。
    [[nodiscard]] virtual std::expected<TextCell, TextLayoutFailure>
    cell(const TextLayoutRequest &request) = 0;
};
} // namespace nenenib::application
