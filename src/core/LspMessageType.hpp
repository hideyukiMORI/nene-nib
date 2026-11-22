#pragma once

#include <cstdint>

namespace nenenib::core
{
// LSP 3.17 の MessageType（`window/showMessage` と `window/logMessage` の type・ADR 0160 の
// 2026-11-22 の追補）。1 Error・2 Warning・3 Info・4 Log。表に無い数（3.18 の 5 Debug など）は
// log。
enum class LspMessageType : std::uint8_t
{
    error,
    warning,
    info,
    log
};
} // namespace nenenib::core
