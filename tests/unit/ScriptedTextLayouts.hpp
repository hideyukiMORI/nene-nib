#pragma once

#include "TextLayoutPort.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace nenenib::tests
{
// applicationの画面行契約用。1 code point = 10画素の決定的な文字組み。
// 実際の字体/Unicodeクラスタの契約はtests/uiのDirectWriteで検証する。
class ScriptedTextLayouts final : public nenenib::application::TextLayoutPort
{
  public:
    [[nodiscard]] std::expected<std::size_t, nenenib::application::TextLayoutFailure>
    rows(const nenenib::application::TextLayoutRequest &request) override
    {
        ++requests_;
        longest_ = std::max(longest_, request.text.size());
        texts_.insert(request.text.data());
        note(request);
        if (rejected_)
        {
            return std::unexpected(nenenib::application::TextLayoutFailure::unavailable);
        }
        const auto count = nenenib::core::code_point_count(request.text);
        const auto width = columns(request);
        return count == 0 ? 1 : (count - 1) / width + 1;
    }

    [[nodiscard]] std::expected<nenenib::core::TextLocation,
                                nenenib::application::TextLayoutFailure>
    locate(const nenenib::application::TextLayoutRequest &request,
           nenenib::core::DisplayCaret caret) override
    {
        ++requests_;
        longest_ = std::max(longest_, request.text.size());
        note(request);
        if (rejected_)
        {
            return std::unexpected(nenenib::application::TextLayoutFailure::unavailable);
        }
        const auto count = nenenib::core::code_point_count(request.text);
        const auto at = std::min(caret.column.value - 1, count);
        const auto width = columns(request);
        const bool previous =
            at > 0 && at % width == 0 &&
            (at == count || caret.affinity == nenenib::core::CaretAffinity::trailing);
        return nenenib::core::TextLocation{at / width - (previous ? 1U : 0U),
                                           static_cast<float>(previous ? width : at % width) *
                                               10.0F};
    }

    [[nodiscard]] std::expected<nenenib::core::DisplayCaret,
                                nenenib::application::TextLayoutFailure>
    hit(const nenenib::application::TextLayoutRequest &request,
        nenenib::core::TextLocation point) override
    {
        ++requests_;
        longest_ = std::max(longest_, request.text.size());
        note(request);
        if (rejected_)
        {
            return std::unexpected(nenenib::application::TextLayoutFailure::unavailable);
        }
        const auto count = nenenib::core::code_point_count(request.text);
        const auto width = columns(request);
        const auto x = std::clamp(point.x / 10.0F + 0.5F, 0.0F, static_cast<float>(width));
        const auto column = static_cast<std::size_t>(x);
        const auto begin = std::min(point.row, count / width) * width;
        return nenenib::core::DisplayCaret{
            nenenib::core::Column{begin + std::min(column, count - begin) + 1},
            column == width ? nenenib::core::CaretAffinity::trailing
                            : nenenib::core::CaretAffinity::leading};
    }

    // 格子の 1 升（ADR 0140）。幅は 1 code point と同じ 10 画素、高さは 20 画素。rows / locate /
    // hit の回数（requests）には数えず、別に数える。
    [[nodiscard]] std::expected<nenenib::application::TextCell,
                                nenenib::application::TextLayoutFailure>
    cell(const nenenib::application::TextLayoutRequest &) override
    {
        ++cells_;
        if (rejected_)
        {
            return std::unexpected(nenenib::application::TextLayoutFailure::unavailable);
        }
        return nenenib::application::TextCell{10.0F, 20.0F};
    }

    [[nodiscard]] std::size_t cells() const noexcept
    {
        return cells_;
    }

    void reject() noexcept
    {
        rejected_ = true;
    }
    [[nodiscard]] std::size_t requests() const noexcept
    {
        return requests_;
    }
    // rows が受けた表示文字列の番地の数。同じ行の表示値を使い回せば 1 つ（#529）。
    [[nodiscard]] std::size_t row_texts() const noexcept
    {
        return texts_.size();
    }

    // rows / locate / hit に渡った文字列の最長のバイト数（巨大な行の窓の契約・ADR 0139）。
    [[nodiscard]] std::size_t longest() const noexcept
    {
        return longest_;
    }

    void forget_longest() noexcept
    {
        longest_ = 0;
    }

    // 本物の BodyTextLayouts と同じく、書式（字・折り返し・幾何）が変わると保持を捨てると見なした
    // ときに新しく作られる layout の数（ADR 0114 の決定 7・ADR 0128）。
    [[nodiscard]] std::size_t made() const noexcept
    {
        return made_;
    }

  private:
    using LayoutStyle = std::tuple<float, std::string, nenenib::core::LineWrapping,
                                   nenenib::core::TextViewGeometry>;

    void note(const nenenib::application::TextLayoutRequest &request)
    {
        LayoutStyle style{request.settings.font_size.points(),
                          std::string(request.settings.font_family.text()),
                          request.settings.wrapping, request.geometry};
        if (style != style_)
        {
            style_ = std::move(style);
            made_texts_.clear();
        }
        if (std::ranges::find(made_texts_, request.text) == made_texts_.end())
        {
            made_texts_.emplace_back(request.text);
            ++made_;
        }
    }

    [[nodiscard]] static std::size_t columns(const nenenib::application::TextLayoutRequest &request)
    {
        // 巨大な行の窓は折り返さない（ADR 0139 の決定 2）。
        if (request.settings.wrapping == nenenib::core::LineWrapping::no_wrap ||
            request.geometry.width <= 0 || request.window.active)
        {
            return nenenib::core::code_point_count(request.text) + 1;
        }
        return static_cast<std::size_t>(std::max(request.geometry.width / 10, 1));
    }

    std::size_t requests_ = 0;
    std::size_t longest_ = 0;
    std::size_t cells_ = 0;
    std::set<const char *> texts_;
    std::size_t made_ = 0;
    LayoutStyle style_{};
    std::vector<std::string> made_texts_;
    bool rejected_ = false;
};
} // namespace nenenib::tests
