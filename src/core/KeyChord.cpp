#include "KeyChord.hpp"

#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] std::string_view key_name(OperationKey key) noexcept
{
    switch (key)
    {
    case OperationKey::o:
        return "O";
    case OperationKey::s:
        return "S";
    case OperationKey::t:
        return "T";
    case OperationKey::w:
        return "W";
    case OperationKey::p:
        return "P";
    case OperationKey::d:
        return "D";
    case OperationKey::z:
        return "Z";
    case OperationKey::y:
        return "Y";
    case OperationKey::f1:
        return "F1";
    case OperationKey::f4:
        return "F4";
    case OperationKey::tab:
        return "Tab";
    case OperationKey::plus:
        return "+";
    case OperationKey::minus:
        return "-";
    case OperationKey::zero:
        return "0";
    }
    std::unreachable();
}
} // namespace

std::string key_chord_label(KeyChord chord)
{
    std::string label;
    if (chord.control)
    {
        label += "Ctrl+";
    }
    if (chord.shift)
    {
        label += "Shift+";
    }
    label += key_name(chord.key);
    return label;
}
} // namespace nenenib::core
