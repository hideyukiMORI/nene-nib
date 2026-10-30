#include "PrintPaperSizes.hpp"

#include "DevicePixels.hpp"

#include <array>
#include <cstdint>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
// 用紙の番号 → 幅と長さ（0.1 mm）。番号は wingdi.h の DMPAPER_*。
constexpr std::array<std::array<std::int32_t, 3>, 9> paper_sizes{{
    {DMPAPER_LETTER, 2159, 2794},
    {DMPAPER_TABLOID, 2794, 4318},
    {DMPAPER_LEGAL, 2159, 3556},
    {DMPAPER_EXECUTIVE, 1842, 2667},
    {DMPAPER_A3, 2970, 4200},
    {DMPAPER_A4, 2100, 2970},
    {DMPAPER_A5, 1480, 2100},
    {DMPAPER_B4, 2570, 3640},
    {DMPAPER_B5, 1820, 2570},
}};
constexpr std::int32_t a4_width = 2100;
constexpr std::int32_t a4_length = 2970;
constexpr std::int32_t tenths_per_inch = 254;

// 0.1 mm → DIP（四捨五入）。
[[nodiscard]] constexpr std::int32_t to_dips(std::int32_t tenths) noexcept
{
    const auto dpi = static_cast<std::int32_t>(core::reference_dpi);
    return (tenths * dpi + tenths_per_inch / 2) / tenths_per_inch;
}

[[nodiscard]] bool has(const DEVMODEW &mode, DWORD field) noexcept
{
    return (mode.dmFields & field) != 0;
}

[[nodiscard]] std::pair<std::int32_t, std::int32_t> tenths_of(const DEVMODEW &mode) noexcept
{
    if (has(mode, DM_PAPERWIDTH) && has(mode, DM_PAPERLENGTH) && mode.dmPaperWidth > 0 &&
        mode.dmPaperLength > 0)
    {
        return {mode.dmPaperWidth, mode.dmPaperLength};
    }
    for (const auto &[code, width, length] : paper_sizes)
    {
        if (has(mode, DM_PAPERSIZE) && mode.dmPaperSize == code)
        {
            return {width, length};
        }
    }
    return {a4_width, a4_length};
}
} // namespace

PrintPaper print_paper(const DEVMODEW &mode) noexcept
{
    auto [width, length] = tenths_of(mode);
    if (has(mode, DM_ORIENTATION) && mode.dmOrientation == DMORIENT_LANDSCAPE)
    {
        std::swap(width, length);
    }
    return PrintPaper{to_dips(width), to_dips(length)};
}
} // namespace nenenib::ui::win32
