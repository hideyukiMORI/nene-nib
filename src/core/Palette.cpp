#include "Palette.hpp"

#include <utility>

namespace nenenib::core
{
Palette palette_for(Appearance appearance) noexcept
{
    switch (appearance)
    {
    case Appearance::light:
        return Palette{RgbColor{0xF4, 0xF5, 0xF7}, RgbColor{0x1B, 0x1F, 0x24}};
    // dark は Ubuntu 端末の深い茄子色（#300A24）と淡い灰（#EEEEEC）。施主 hide
    // の決定（2026-09-15）。
    case Appearance::dark:
        return Palette{RgbColor{0x30, 0x0A, 0x24}, RgbColor{0xEE, 0xEE, 0xEC}};
    }
    std::unreachable();
}
} // namespace nenenib::core
