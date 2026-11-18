#pragma once

#include "LspPosition.hpp"
#include "TextPosition.hpp"

#include <variant>

namespace nenenib::application
{
// 開いた文書の中の着地の位置（ADR 0167 の決定 5・ADR 0190 の決定 7）。全文検索の当たりは行と桁
// （core::TextPosition）、言語サーバーの定義の場所は Position（core::LspPosition・開いた後の本文で
// lsp_offset_of の 1 本が写す・行が本文の外なら末尾）。選択肢が増えたら写し先が落ちる（CPP-002）。
using LandingPlace = std::variant<core::TextPosition, core::LspPosition>;
} // namespace nenenib::application
