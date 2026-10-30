#include "PrintPages.hpp"

#include <algorithm>

namespace nenenib::core
{
namespace
{
// 1 インチは 25.4 mm。整数のまま割るため 10 倍して 254 で割る。
constexpr std::uint64_t tenths_of_millimetre_per_inch = 254;
} // namespace

std::uint32_t print_margin(std::uint32_t dpi) noexcept
{
    const std::uint64_t tenths = std::uint64_t{print_margin_millimetres} * 10U;
    return static_cast<std::uint32_t>((tenths * dpi + tenths_of_millimetre_per_inch / 2U) /
                                      tenths_of_millimetre_per_inch);
}

std::uint32_t print_rows_per_page(const PrintSheet &sheet) noexcept
{
    const std::uint64_t taken =
        std::uint64_t{print_margin(sheet.dpi)} * 2U + std::uint64_t{sheet.header_height};
    if (sheet.row_height == 0U || taken >= sheet.height)
    {
        return 1U;
    }
    const std::uint64_t rows = (std::uint64_t{sheet.height} - taken) / sheet.row_height;
    return static_cast<std::uint32_t>(std::max<std::uint64_t>(rows, 1U));
}

std::vector<PrintPage> print_pages(std::size_t rows, std::uint32_t rows_per_page)
{
    const std::size_t step = std::max<std::size_t>(rows_per_page, 1U);
    std::vector<PrintPage> pages;
    pages.reserve(rows / step + 1U);
    std::size_t first = 0;
    do
    {
        const std::size_t count = std::min(step, rows - first);
        pages.push_back(PrintPage{first, count});
        first += count;
    } while (first < rows);
    return pages;
}

std::string print_page_label(std::size_t number, std::size_t count)
{
    return std::to_string(number) + " / " + std::to_string(count);
}
} // namespace nenenib::core
