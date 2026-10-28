#pragma once

#include <string>

namespace nenenib::application
{
// サーバーから届いた JSON-RPC の本文 1 通（ADR 0160 の決定 1）。見出しは adapter が外してある。
// 公開 aggregate（CPP-003）。
struct ServerMessage
{
    std::string body;
};
} // namespace nenenib::application
