#pragma once

#include <cstdint>

namespace nenenib::application
{
// 言語サーバーの寿命の段（ADR 0160 の決定 6）。idle は子が居ない、starting は start の後で
// ServerStarted を待つ、initializing は `initialize` の答えを待つ（答えまで何も送らない）、
// running は `initialized` を送った後、shutting_down は `shutdown` の答えを待つ（答えで `exit`
// を送って stop する）。増えたら写し先の switch がコンパイルで落ちる（CPP-002）。
enum class LanguageServerPhase : std::uint8_t
{
    idle,
    starting,
    initializing,
    running,
    shutting_down
};
} // namespace nenenib::application
