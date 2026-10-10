#pragma once

#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests::performance
{
enum class ProbeWorkload : std::uint8_t
{
    controller_open,
    buffer_create,
    controller_insert,
    display_long,
    insert_after_delete_small,
    insert_after_delete_large,
    validate_ascii,
    validate_japanese,
    vim_register_small,
    vim_register_large,
    vim_record_small,
    vim_record_large,
    scattered_crlf_lines,
    scattered_lf_lines,
    long_position,
    scattered_position,
    listed_name,
    listed_location,
    palette_narrow,
    palette_left,
    codepage_japanese,
    utf16_japanese,
    utf16_ascii,
    utf16_supplementary,
    controller_save,
    controller_save_bom,
    erase_scattered_head,
    erase_scattered_middle,
    erase_scattered_tail,
    erase_scattered_all,
    erase_single_middle,
    offset_long_head,
    offset_long_middle,
    offset_long_end,
    offset_scattered_middle,
    search_forward_head,
    search_forward_middle,
    search_forward_tail,
    search_backward_head,
    search_backward_middle,
    search_backward_tail
};

[[nodiscard]] std::optional<ProbeWorkload> workload_of(std::string_view name) noexcept;
[[nodiscard]] std::string input_of(ProbeWorkload workload);
[[nodiscard]] std::uint64_t checksum_of(std::string_view bytes) noexcept;
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_workload(ProbeWorkload workload, const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
