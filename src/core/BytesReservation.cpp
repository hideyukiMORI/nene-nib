#include "BytesReservation.hpp"

#include "SealedBytes.hpp"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::core
{
BytesReservation::BytesReservation(std::string bytes) noexcept : bytes_(std::move(bytes)) {}

// 長さを宣言するだけで中身は書かない（resize_and_overwrite の約束・決定 8）。読む口が無いので
// 埋めていない分が外へ出ることはなく、seal は filled の長さに切り詰める。
BytesReservation BytesReservation::reserve(std::size_t declared)
{
    std::string bytes;
    if (declared != 0)
    {
        bytes.resize_and_overwrite(declared,
                                   [](char *, std::size_t size) noexcept { return size; });
    }
    return BytesReservation(std::move(bytes));
}

std::span<char> BytesReservation::window(std::size_t length) noexcept
{
    window_ = (std::min)(length, bytes_.size() - filled_);
    return std::span<char>(bytes_.data() + filled_, window_);
}

void BytesReservation::commit(std::size_t written) noexcept
{
    const std::size_t accepted = (std::min)(written, window_);
    window_ = 0;
    if (validation_)
    {
        validation_ =
            validation_.value().with_bytes(std::string_view(bytes_.data() + filled_, accepted));
    }
    filled_ += accepted;
}

SealedBytes BytesReservation::seal() &&
{
    window_ = 0;
    bytes_.resize(filled_);
    std::optional<std::size_t> code_points;
    if (validation_)
    {
        const auto finished = validation_.value().finish();
        if (finished)
        {
            code_points = finished.value();
        }
    }
    return SealedBytes(std::move(bytes_), code_points);
}
} // namespace nenenib::core
