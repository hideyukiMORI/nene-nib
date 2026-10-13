#pragma once

#include "FileFailure.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <type_traits>

namespace nenenib::adapters::win32
{
[[nodiscard]] constexpr std::size_t file_read_request_size(std::size_t remaining) noexcept
{
    return (std::min)(remaining,
                      static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()));
}

// 同期operationは返却長か失敗だけを返し、借用spanを保持しない（ADR 0124）。
template <typename Operation>
[[nodiscard]] std::expected<void, application::FileFailure>
complete_file_read_into(std::span<char> target, Operation &operation) noexcept
{
    using Failure = application::FileFailure;
    static_assert(std::is_nothrow_invocable_r_v<std::expected<std::size_t, Failure>, Operation &,
                                                std::span<char>>);
    std::size_t received = 0;
    while (received < target.size())
    {
        const auto request = file_read_request_size(target.size() - received);
        const auto read = operation(target.subspan(received, request));
        if (!read || read.value() == 0 || read.value() > request)
        {
            return std::unexpected(Failure::unreadable);
        }
        received += read.value();
    }
    return {};
}

template <typename Operation>
[[nodiscard]] std::expected<std::string, application::FileFailure>
complete_file_read(std::size_t declared_size, Operation operation)
{
    using Failure = application::FileFailure;
    std::expected<void, Failure> outcome;
    std::string bytes;
    bytes.resize_and_overwrite(declared_size,
                               [&](char *data, std::size_t) noexcept
                               {
                                   outcome = complete_file_read_into(
                                       std::span<char>(data, declared_size), operation);
                                   return outcome.has_value() ? declared_size : std::size_t{0};
                               });
    if (!outcome)
    {
        return std::unexpected(outcome.error());
    }
    return bytes;
}
} // namespace nenenib::adapters::win32
