#include "PlacementCleanup.hpp"

#include <windows.h>

#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
// ReplaceFileW の失敗の番号は OS の開いた値なので、閉じた選択肢の switch にしない。文書
// （learn.microsoft.com の ReplaceFileW の Return value）に名前のある 2 つだけを選び、残りは
// 1 か所の既定に閉じる（ADR 0135 の追補・#1689）。
[[nodiscard]] PlacementCleanup replacement_cleanup(std::uint32_t error) noexcept
{
    if (error == ERROR_UNABLE_TO_MOVE_REPLACEMENT)
    {
        return PlacementCleanup::move_temporary;
    }
    if (error == ERROR_UNABLE_TO_MOVE_REPLACEMENT_2)
    {
        return PlacementCleanup::keep_temporary;
    }
    // 既定: ERROR_UNABLE_TO_REMOVE_REPLACED（1175）とほかの番号は、文書では元のファイルと一時
    // ファイルがどちらも元の名前のまま残る。元のファイルが無事なので一時ファイルを消す。
    return PlacementCleanup::remove_temporary;
}
} // namespace

PlacementCleanup placement_cleanup(std::uint32_t error, application::FileWriteMode mode) noexcept
{
    switch (mode)
    {
    case application::FileWriteMode::replace:
        return replacement_cleanup(error);
    case application::FileWriteMode::create_new:
        return PlacementCleanup::remove_temporary;
    }
    std::unreachable();
}
} // namespace nenenib::adapters::win32
