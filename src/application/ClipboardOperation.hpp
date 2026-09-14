#pragma once

#include <cstdint>

namespace nenenib::application
{
// クリップボードに対してすることの閉じた一覧（CPP-002）。
enum class ClipboardOperation : std::uint8_t
{
    copy,
    cut,
    paste
};
} // namespace nenenib::application
