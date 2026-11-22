#pragma once

#include "ServerErrorOutput.hpp"
#include "ServerExited.hpp"
#include "ServerFailed.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"

#include <variant>

namespace nenenib::application
{
// 言語サーバーの port が collect で渡す出来事の閉じた和型（ADR 0160 の決定 1・2026-11-22 の追補で
// 標準エラーの塊 ServerErrorOutput を足した）。読む所は std::visit。
using ServerEvent =
    std::variant<ServerStarted, ServerMessage, ServerExited, ServerFailed, ServerErrorOutput>;
} // namespace nenenib::application
