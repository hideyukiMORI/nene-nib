#pragma once

#include "EditMode.hpp"

#include <cstdint>

namespace nenenib::core
{
enum class BookmarkKey : std::uint8_t
{
    control_d,
    control_shift_d
};

// VimのCtrl+DはVimへ残す。登録の鍵はモードごとに1つ（D36・ADR 0063）。
[[nodiscard]] bool toggles_bookmark(BookmarkKey key, EditMode mode) noexcept;
} // namespace nenenib::core
