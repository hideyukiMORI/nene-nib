#pragma once

#include <algorithm>
#include <cstddef>
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
class SealedBytes final
{
  public:
    // fill は書いた長さを返し、例外を投げない。span の大きさが declared で、それを超えて書けない。
    // declared が 0 なら fill を呼ばず空の列（ADR 0124 決定 3）。
    template <class Fill> [[nodiscard]] static SealedBytes filled(std::size_t declared, Fill &&fill)
    {
        static_assert(std::is_nothrow_invocable_r_v<std::size_t, Fill &, std::span<char>>);
        std::string bytes;
        if (declared != 0)
        {
            bytes.resize_and_overwrite(
                declared, [&fill](char *data, std::size_t size) noexcept
                { return (std::min)(fill(std::span<char>(data, size)), size); });
        }
        return SealedBytes(std::move(bytes));
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

  private:
    // 所有を取り出せるのは判定済みの列へ移す DetectedText だけ（ADR 0141 決定 3）。
    friend class DetectedText;
    explicit SealedBytes(std::string bytes) noexcept : bytes_(std::move(bytes)) {}
    [[nodiscard]] std::string release() && noexcept
    {
        return std::move(bytes_);
    }
    std::string bytes_;
};
} // namespace nenenib::core
