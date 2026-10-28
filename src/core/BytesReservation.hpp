#pragma once

#include "TextFailure.hpp"
#include "Utf8Validation.hpp"

#include <cstddef>
#include <expected>
#include <span>
#include <string>

namespace nenenib::core
{
class SealedBytes;

// 封印の前の列を予約として持ち回る（ADR 0141 の追補 8・9）。move だけできる。reserve で宣言の
// 長さを確保し（零で埋めない）、window で未だ埋めていない先頭からの可変の窓を借り、commit で書いた
// 長さを確定する。窓は次の commit か seal まで有効で、同じ仕事の中でだけ持つ。読む口は無く、
// 封印の前の列は誰にも見えない。commit は確定した分を UTF-8 の流れる検証に通し、seal がその結果を
// SealedBytes の非公開の欄に添える（全文の検証は 1 回）。
class BytesReservation final
{
  public:
    [[nodiscard]] static BytesReservation reserve(std::size_t declared);

    BytesReservation(const BytesReservation &) = delete;
    BytesReservation &operator=(const BytesReservation &) = delete;
    BytesReservation(BytesReservation &&) noexcept = default;
    BytesReservation &operator=(BytesReservation &&) noexcept = default;
    ~BytesReservation() = default;

    // まだ埋めていない先頭から length の窓。宣言の残り（declared − filled）を超えない。
    [[nodiscard]] std::span<char> window(std::size_t length) noexcept;
    // 直前の窓に書いた長さを確定する。窓を超えた値は窓の長さに切り詰める。
    void commit(std::size_t written) noexcept;
    // 確定した長さに切り詰めて封印する。予約はこれで尽きる。
    [[nodiscard]] SealedBytes seal() &&;

  private:
    explicit BytesReservation(std::string bytes) noexcept;

    std::string bytes_;
    std::size_t filled_ = 0;
    std::size_t window_ = 0;
    std::expected<Utf8Validation, TextFailure> validation_ = Utf8Validation::empty();
};
} // namespace nenenib::core
