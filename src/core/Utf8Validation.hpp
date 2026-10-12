#pragma once

#include "TextFailure.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <string_view>

namespace nenenib::core
{
// 分割したUTF-8検証の進捗。本文の所有値でも、別bytesの検証済み証明でもない（ADR 0122）。
class Utf8Validation final
{
  public:
    [[nodiscard]] static Utf8Validation empty() noexcept
    {
        return Utf8Validation{};
    }

    // 入力は呼出し中だけ借用する。未完の末尾だけを所有し、元の進捗は変更しない。
    [[nodiscard]] std::expected<Utf8Validation, TextFailure>
    with_bytes(std::string_view bytes) const noexcept
    {
        return continued_utf8(*this, bytes);
    }

    // 空断片はEOFではない。ここで未完列を拒否し、完成したcode pointの総数を返す。
    [[nodiscard]] std::expected<std::size_t, TextFailure> finish() const noexcept
    {
        if (pending_size_ != 0)
        {
            return std::unexpected(TextFailure::invalid_utf8);
        }
        return code_points_;
    }

  private:
    // 状態の更新も既存Utf8.cppの走査だけが行う。外へ可変領域を貸さない。
    friend std::expected<Utf8Validation, TextFailure>
    continued_utf8(const Utf8Validation &before, std::string_view bytes) noexcept;

    Utf8Validation() = default;
    // 呼出し間では最大3 bytes。再開するときだけ4 bytesまで埋め、完成した列を取り出す。
    std::array<char, 4> pending_{};
    std::size_t pending_size_ = 0;
    std::size_t bytes_seen_ = 0;
    std::size_t code_points_ = 0;
};
} // namespace nenenib::core
