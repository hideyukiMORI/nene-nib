#pragma once

#include "VimPattern.hpp"
#include "VimPatternCase.hpp"
#include "VimPatternFailure.hpp"
#include "VimSearchDirection.hpp"
#include "VimSmartcase.hpp"

#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 確定した検索の文字列・向き・解析成功/失敗を一緒に所有する（ADR 0102）。
// 本文の一致や検索の起点は持たず、既存parserの値だけを不変共有する。
// 大小の旗（#1041）は検索の時点の `ignorecase` `smartcase` で、`*` `#` は smartcase を見ない。
// 設定が変わったら recased で同じ文字列を新しい旗で解き直す（`n` は今の設定で探す・実測）。
class VimSearchSnapshot final
{
  public:
    VimSearchSnapshot(const VimSearchSnapshot &) = default;
    VimSearchSnapshot &operator=(const VimSearchSnapshot &) = default;

    [[nodiscard]] static VimSearchSnapshot from(std::string_view text, VimSearchDirection direction,
                                                VimPatternCase letter_case, VimSmartcase smartcase);
    // 同じ文字列・向き・smartcase の扱いを、新しい旗で解き直した値。
    [[nodiscard]] VimSearchSnapshot recased(VimPatternCase letter_case) const;
    [[nodiscard]] std::string_view text() const & noexcept;
    std::string_view text() const && = delete;
    [[nodiscard]] VimSearchDirection direction() const noexcept;
    [[nodiscard]] VimSmartcase smartcase() const noexcept;
    [[nodiscard]] const std::expected<VimPattern, VimPatternFailure> &parsed() const & noexcept;
    const std::expected<VimPattern, VimPatternFailure> &parsed() const && = delete;

  private:
    struct Storage
    {
        std::string text;
        VimSearchDirection direction;
        VimSmartcase smartcase;
        std::expected<VimPattern, VimPatternFailure> parsed;
    };

    explicit VimSearchSnapshot(std::shared_ptr<const Storage> storage);
    std::shared_ptr<const Storage> storage_;
};
} // namespace nenenib::core
