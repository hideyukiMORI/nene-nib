#include "BookmarkKey.hpp"

#include "EditorOperation.hpp"
#include "KeyChord.hpp"
#include "OperationBindings.hpp"
#include "OperationKey.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] constexpr KeyChord chord_of(BookmarkKey key) noexcept
{
    switch (key)
    {
    case BookmarkKey::control_d:
        return KeyChord{true, false, OperationKey::d};
    case BookmarkKey::control_shift_d:
        return KeyChord{true, true, OperationKey::d};
    }
    std::unreachable();
}
} // namespace

// モードごとの鍵は割り当ての表 operation_bindings の行が決める（ADR 0078 の決定 4）。
bool toggles_bookmark(BookmarkKey key, EditMode mode) noexcept
{
    return operation_for(chord_of(key), mode) == EditorOperation::toggle_bookmark;
}
} // namespace nenenib::core
