#pragma once

#include "TextLayoutPort.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <set>

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
        texts_.insert(request.text.data());
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

    // 格子の 1 升（ADR 0140・試作）。1 code point = 10画素と同じ。
    [[nodiscard]] std::expected<float, nenenib::application::TextLayoutFailure>
    cell(const nenenib::application::TextLayoutRequest &) override
    {
        if (rejected_)
        {
            return std::unexpected(nenenib::application::TextLayoutFailure::unavailable);
        }
        return 10.0F;
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

  private:
    [[nodiscard]] static std::size_t columns(const nenenib::application::TextLayoutRequest &request)
    {
        if (request.settings.wrapping == nenenib::core::LineWrapping::no_wrap ||
            request.geometry.width <= 0)
        {
            return nenenib::core::code_point_count(request.text) + 1;
        }
        return static_cast<std::size_t>(std::max(request.geometry.width / 10, 1));
    }

    std::size_t requests_ = 0;
    std::set<const char *> texts_;
    bool rejected_ = false;
};
} // namespace nenenib::tests
