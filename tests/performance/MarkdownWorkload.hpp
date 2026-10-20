#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// 16 MiB の「普通の Markdown」を parse_markdown で 1 回読み、平らな列を組み立てる（#765・ADR 0151
// の決定 12）。入力は md4c の probe（docs/quality/md4c-probe-2026-10-21.md の (i)）と同じ生成の形。
enum class MarkdownWorkload : std::uint8_t
{
    mixed_16mib
};
} // namespace nenenib::tests::performance
