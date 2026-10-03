#include "BookmarkKey.hpp"

#include <utility>

namespace nenenib::core
{
bool toggles_bookmark(BookmarkKey key, EditMode mode) noexcept
{
    switch (mode)
    {
    case EditMode::ordinary:
        switch (key)
        {
        case BookmarkKey::control_d:
            return true;
        case BookmarkKey::control_shift_d:
            return false;
        }
        break;
    case EditMode::vim:
        switch (key)
        {
        case BookmarkKey::control_d:
            return false;
        case BookmarkKey::control_shift_d:
            return true;
        }
        break;
    }
    std::unreachable();
}
} // namespace nenenib::core
