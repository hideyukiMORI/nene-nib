#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
enum class StatusItemsWorkload : std::uint8_t
{
    utf8_crlf,
    bom_lf,
    sjis_crlf,
    max_utf8_lf,
    large_bom_crlf,
    sjis_lf
};
} // namespace nenenib::tests::performance
