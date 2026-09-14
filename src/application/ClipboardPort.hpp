#pragma once

#include "ClipboardFailure.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::application
{
// OS のクリップボードへの唯一の入口（ADR 0009 の決定 5）。中核はクリップボードを知らない。
// やり取りは UTF-8 で、UTF-16 との変換は実装（src/adapters/win32）の中だけで起きる（CPP-014）。
class ClipboardPort
{
  public:
    ClipboardPort() = default;
    virtual ~ClipboardPort() = default;
    ClipboardPort(const ClipboardPort &) = delete;
    ClipboardPort(ClipboardPort &&) = delete;
    ClipboardPort &operator=(const ClipboardPort &) = delete;
    ClipboardPort &operator=(ClipboardPort &&) = delete;

    [[nodiscard]] virtual std::expected<void, ClipboardFailure> write(std::string_view utf8) = 0;
    [[nodiscard]] virtual std::expected<std::string, ClipboardFailure> read() = 0;
};
} // namespace nenenib::application
