#include "DisplayFormLabel.hpp"

#include <utility>

namespace nenenib::core
{
std::string_view display_form_label(DisplayForm display) noexcept
{
    switch (display)
    {
    case DisplayForm::text:
        return "";
    case DisplayForm::grid:
        return "grid";
    }
    std::unreachable();
}
} // namespace nenenib::core
