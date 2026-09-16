#include "ModeLabel.hpp"

#include <utility>

namespace nenenib::core
{
std::string_view mode_label(EditMode mode, VimMode vim) noexcept
{
    switch (mode)
    {
    case EditMode::ordinary:
        return "通常";
    case EditMode::vim:
        break;
    }
    switch (vim)
    {
    case VimMode::normal:
        return "NORMAL";
    case VimMode::insert:
        return "INSERT";
    }
    std::unreachable();
}
} // namespace nenenib::core
