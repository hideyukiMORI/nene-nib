#pragma once

#include "ServerExited.hpp"
#include "ServerFailed.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"

#include <variant>

namespace nenenib::application
{
// 言語サーバーの port が collect で渡す出来事の閉じた和型（ADR 0160 の決定 1）。読む所は
// std::visit。
using ServerEvent = std::variant<ServerStarted, ServerMessage, ServerExited, ServerFailed>;
} // namespace nenenib::application
