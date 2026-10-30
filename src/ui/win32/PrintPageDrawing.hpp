#pragma once

#include "PrintFailure.hpp"
#include "PrintPageInput.hpp"

#include <d2d1_1.h>
#include <expected>
#include <span>

namespace nenenib::ui::win32
{
// 印刷の 1 ページを描く 1 本（ADR 0177 の決定 3〜5・8）。白地にヘッダ（左に題名・右に「n / N」・
// muted）と行番号（gutter）と本文（構文の色・置き換えた文字は muted）を、画面と同じ
// BodyTextPainter の字形で描く。選択・キャレット・検索の面・IME は描かない。呼び手が BeginDraw と
// EndDraw を持つ。DirectWrite が答えなければ rendering_failed。
[[nodiscard]] std::expected<void, application::PrintFailure>
draw_print_page(ID2D1DeviceContext &context, const PrintPageInput &input);

// ページの画面行 [page.first_row, page.first_row + row_count) と交わる行の区間（lines は first_row
// の昇順・二分探索）。1 ページで見る行はページの行数に比例し、文書の行数に依らない（#1047 の
// レビュー R4-1）。空のページは空の区間。
[[nodiscard]] std::span<const PrintLine> print_page_lines(std::span<const PrintLine> lines,
                                                          const core::PrintPage &page) noexcept;
} // namespace nenenib::ui::win32
