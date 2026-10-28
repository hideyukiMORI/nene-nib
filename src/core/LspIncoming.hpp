#pragma once

#include "LspNotification.hpp"
#include "LspRequest.hpp"
#include "LspResponse.hpp"

#include <variant>

namespace nenenib::core
{
// サーバーから来た 1 通の閉じた和型（ADR 0160 の決定 5）。読む所は std::visit で分ける（ADR
// 0009）。
using LspIncoming = std::variant<LspResponse, LspNotification, LspRequest>;
} // namespace nenenib::core
