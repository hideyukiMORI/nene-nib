#pragma once

#include "LanguageServerSlot.hpp"
#include "ServerEvent.hpp"

namespace nenenib::application
{
// collect が渡す出来事 1 つと、それを出した子の席（ADR 0160 の 2026-11-20 の追補）。公開
// aggregate（CPP-003）。
struct LanguageServerEvent
{
    LanguageServerSlot slot;
    ServerEvent event;
};
} // namespace nenenib::application
