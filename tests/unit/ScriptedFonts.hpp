#pragma once

#include "FontFamilyFailure.hpp"
#include "FontFamilyPort.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// 字体の一覧の替え玉（#597）。読んだ回数を数える（verify_quiet_paths）。
class ScriptedFonts final : public nenenib::application::FontFamilyPort
{
  public:
    using Listing =
        std::expected<std::vector<std::string>, nenenib::application::FontFamilyFailure>;
    explicit ScriptedFonts(Listing listing = std::vector<std::string>{"Consolas", "Cascadia Mono"})
        : listing_(std::move(listing))
    {
    }
    [[nodiscard]] Listing list() override
    {
        ++lists_;
        return listing_;
    }
    void serve(Listing listing)
    {
        listing_ = std::move(listing);
    }
    [[nodiscard]] std::size_t lists() const noexcept
    {
        return lists_;
    }

  private:
    Listing listing_;
    std::size_t lists_ = 0;
};
} // namespace nenenib::tests
