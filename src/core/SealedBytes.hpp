#pragma once

#include "BytesReservation.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nenenib::core
{
class DetectedText;

// core が予約し、埋め終えたら可変の口を持たない所有の列（ADR 0141 決定 1）。move だけできる。
// 埋める側は filled の fill の中でだけ span を借り、書いた長さを返す。読む口は view() だけ。
// 作るのは BytesReservation の封印だけで、filled はその reserve → window → commit → seal の
// 薄い入口（追補 8）。封印は予約が流した UTF-8 の検証の結果を添える（追補 9）。
class SealedBytes final
{
  public:
    // fill は書いた長さを返し、例外を投げない。span の大きさが declared で、それを超えて書けない。
    // declared が 0 なら fill を呼ばず空の列（ADR 0124 決定 3）。
    template <class Fill> [[nodiscard]] static SealedBytes filled(std::size_t declared, Fill &&fill)
    {
        static_assert(std::is_nothrow_invocable_r_v<std::size_t, Fill &, std::span<char>>);
        auto reservation = BytesReservation::reserve(declared);
        if (declared != 0)
        {
            const auto target = reservation.window(declared);
            reservation.commit(fill(target));
        }
        return std::move(reservation).seal();
    }

    SealedBytes(const SealedBytes &) = delete;
    SealedBytes &operator=(const SealedBytes &) = delete;
    SealedBytes(SealedBytes &&) noexcept = default;
    SealedBytes &operator=(SealedBytes &&) noexcept = default;
    ~SealedBytes() = default;

    [[nodiscard]] std::string_view view() const & noexcept
    {
        return bytes_;
    }
    [[nodiscard]] std::string_view view() const && = delete;
    [[nodiscard]] std::size_t size() const noexcept
    {
        return bytes_.size();
    }
    // 予約が流した検証で列の全体が正しい UTF-8 だったときの code point の数。違えば値なし（追補
    // 9）。 列へ届く口ではない（数だけ）。
    [[nodiscard]] std::optional<std::size_t> utf8_code_points() const noexcept
    {
        return utf8_code_points_;
    }

  private:
    // 所有を取り出せるのは判定済みの列へ移す DetectedText だけ（ADR 0141 決定 3）。作るのは
    // 予約の封印だけ（追補 8）。
    friend class DetectedText;
    friend class BytesReservation;
    SealedBytes(std::string bytes, std::optional<std::size_t> utf8_code_points) noexcept
        : bytes_(std::move(bytes)), utf8_code_points_(utf8_code_points)
    {
    }
    [[nodiscard]] std::string release() && noexcept
    {
        return std::move(bytes_);
    }
    std::string bytes_;
    std::optional<std::size_t> utf8_code_points_;
};
} // namespace nenenib::core
