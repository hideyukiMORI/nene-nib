#pragma once

#include "PrintFailure.hpp"
#include "PrintPageInput.hpp"

#include <d2d1_1.h>
#include <expected>

namespace nenenib::ui::win32
{
// 印刷の 1 ページを描く 1 本（ADR 0177 の決定 3〜5・8）。白地にヘッダ（左に題名・右に「n / N」・
// muted）と行番号（gutter）と本文（構文の色・置き換えた文字は muted）を、画面と同じ
// BodyTextPainter の字形で描く。選択・キャレット・検索の面・IME は描かない。呼び手が BeginDraw と
// EndDraw を持つ。DirectWrite が答えなければ rendering_failed。
[[nodiscard]] std::expected<void, application::PrintFailure>
draw_print_page(ID2D1DeviceContext &context, const PrintPageInput &input);
} // namespace nenenib::ui::win32
