#pragma once

#include <cstdint>

namespace nenenib::application
{
// フォルダの列挙の進み具合（ADR 0062 の決定 6）。閉じた集合（CPP-002）。
// more の後に続きが来る。complete・truncated・failed は最後の batch に 1 つだけ付く。
enum class FolderProgress : std::uint8_t
{
    more,
    complete,
    truncated,
    failed
};
} // namespace nenenib::application
