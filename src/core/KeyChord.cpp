#include "KeyChord.hpp"

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 鍵 → 表示名の表（CPP-012・#1051 で 31 鍵になり switch が関数長を越えた）。OperationKey
// の順に全値を 1 回ずつ持つ（下の static_assert が守る）。
constexpr std::array<std::pair<OperationKey, std::string_view>, 32> operation_key_names{{
    {OperationKey::o, "O"},
    {OperationKey::s, "S"},
    {OperationKey::t, "T"},
    {OperationKey::w, "W"},
    {OperationKey::p, "P"},
    {OperationKey::d, "D"},
    {OperationKey::z, "Z"},
    {OperationKey::y, "Y"},
    {OperationKey::f1, "F1"},
    {OperationKey::f4, "F4"},
    {OperationKey::f6, "F6"},
    {OperationKey::tab, "Tab"},
    {OperationKey::plus, "+"},
    {OperationKey::minus, "-"},
    {OperationKey::zero, "0"},
    {OperationKey::space, "Space"},
    {OperationKey::backslash, "\\"},
    {OperationKey::m, "M"},
    {OperationKey::v, "V"},
    {OperationKey::e, "E"},
    {OperationKey::f5, "F5"},
    {OperationKey::f, "F"},
    {OperationKey::h, "H"},
    {OperationKey::g, "G"},
    {OperationKey::f3, "F3"},
    {OperationKey::n, "N"},
    {OperationKey::r, "R"},
    {OperationKey::x, "X"},
    {OperationKey::c, "C"},
    {OperationKey::a, "A"},
    {OperationKey::delete_key, "Delete"},
    {OperationKey::f12, "F12"},
}};

// OperationKey の値か。値を足すとこの switch がコンパイルで落ち（CPP-002）、足した後は表の行の数と
// 合わなくなって static_assert が落ちる。
[[nodiscard]] consteval bool is_key(OperationKey key) noexcept
{
    switch (key)
    {
    case OperationKey::o:
    case OperationKey::s:
    case OperationKey::t:
    case OperationKey::w:
    case OperationKey::p:
    case OperationKey::d:
    case OperationKey::z:
    case OperationKey::y:
    case OperationKey::f1:
    case OperationKey::f4:
    case OperationKey::f6:
    case OperationKey::tab:
    case OperationKey::plus:
    case OperationKey::minus:
    case OperationKey::zero:
    case OperationKey::space:
    case OperationKey::backslash:
    case OperationKey::m:
    case OperationKey::v:
    case OperationKey::e:
    case OperationKey::f5:
    case OperationKey::f:
    case OperationKey::h:
    case OperationKey::g:
    case OperationKey::f3:
    case OperationKey::n:
    case OperationKey::r:
    case OperationKey::x:
    case OperationKey::c:
    case OperationKey::a:
    case OperationKey::delete_key:
    case OperationKey::f12:
        return true;
    }
    return false;
}

// 表の i 行目が i 番目の値を持ち、表の行の数の次はもう値でない（全値を順に 1 回ずつ）。
[[nodiscard]] consteval bool names_in_order() noexcept
{
    for (std::size_t index = 0; index < operation_key_names.size(); ++index)
    {
        if (std::to_underlying(operation_key_names.at(index).first) != index)
        {
            return false;
        }
    }
    return !is_key(static_cast<OperationKey>(operation_key_names.size()));
}

static_assert(names_in_order(), "operation_key_names has one row for every OperationKey in order");

[[nodiscard]] std::string_view key_name(OperationKey key) noexcept
{
    return operation_key_names.at(std::to_underlying(key)).second;
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
