#pragma once

#include <cstddef>

namespace nenenib::core
{
// タスクリストの項目のチェックボックス（ADR 0155 の追補 22）。checked は印が `x` か `X`、
// mark_offset は本文のバイト位置で `[` の次の 1 バイト（印の ` ` / `x` / `X`）。どちらも単独で
// 妥当な値なので公開 aggregate（CPP-003）。比較は非メンバー。
struct MarkdownTask
{
    bool checked;
    std::size_t mark_offset;
};

[[nodiscard]] inline bool operator==(const MarkdownTask &left, const MarkdownTask &right) noexcept
{
    return left.checked == right.checked && left.mark_offset == right.mark_offset;
}
} // namespace nenenib::core
