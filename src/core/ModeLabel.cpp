#include "ModeLabel.hpp"

#include <utility>

namespace nenenib::core
{
std::string_view mode_label(EditMode mode) noexcept
{
    switch (mode)
    {
    case EditMode::ordinary:
        return "通常";
    case EditMode::vim:
        return "NORMAL";
    }
    std::unreachable();
}
} // namespace nenenib::core
