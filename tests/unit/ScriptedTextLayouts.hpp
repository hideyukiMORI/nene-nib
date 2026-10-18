#pragma once

#include "TextLayoutPort.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace nenenib::tests
{
// applicationの画面行契約用。1 升 = 10画素の決定的な文字組み。表示文字列の code point は表示幅の升
// （column_advance の表・全角 2・Tab は vim_tab_stop の次の止まり・幅 0 は 0）を占め、wrap では幅の
// 升ごとに折る。行末の 1 升に入らない全角は Vim と同じく次の画面行へ送る（#596）。
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
        const auto width = columns(request);
        const auto cells = cell_starts(request.text, width).back();
        return cells == 0 ? 1 : (cells - 1) / width + 1;
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
        const auto width = columns(request);
        const auto starts = cell_starts(request.text, width);
        const auto count = starts.size() - 1;
        const auto at = std::min(caret.column.value - 1, count);
        const auto cell = starts[at];
        const bool previous =
            at > 0 && cell % width == 0 &&
            (at == count || caret.affinity == nenenib::core::CaretAffinity::trailing);
        return nenenib::core::TextLocation{cell / width - (previous ? 1U : 0U),
                                           static_cast<float>(previous ? width : cell % width) *
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
        const auto width = columns(request);
        const auto starts = cell_starts(request.text, width);
        const auto x = std::clamp(point.x / 10.0F + 0.5F, 0.0F, static_cast<float>(width));
        const auto column = static_cast<std::size_t>(x);
        const auto target = (std::min(point.row, starts.back() / width) * width) + column;
        const auto before = std::lower_bound(starts.begin(), std::prev(starts.end()), target);
        return nenenib::core::DisplayCaret{
            nenenib::core::Column{static_cast<std::size_t>(before - starts.begin()) + 1},
            column == width ? nenenib::core::CaretAffinity::trailing
                            : nenenib::core::CaretAffinity::leading};
    }

    // 画面行の問い（#664）。段は幅の升ごとで、桁の先頭側の升の段・本文の末尾は最後の段。範囲は
    // その段と次の段の最初の code point の桁から作る。
    [[nodiscard]] std::expected<nenenib::application::TextRow,
                                nenenib::application::TextLayoutFailure>
    row(const nenenib::application::TextLayoutRequest &request,
        std::variant<nenenib::core::Column, std::size_t> target) override
    {
        ++requests_;
        longest_ = std::max(longest_, request.text.size());
        note(request);
        if (rejected_)
        {
            return std::unexpected(nenenib::application::TextLayoutFailure::unavailable);
        }
        const auto width = columns(request);
        const auto starts = cell_starts(request.text, width);
        const auto count = starts.size() - 1;
        const auto rows = starts.back() == 0 ? 1 : ((starts.back() - 1) / width) + 1;
        const auto *const column = std::get_if<nenenib::core::Column>(&target);
        const auto at = column != nullptr ? std::min(column->value - 1, count) : 0;
        const auto row = std::min(column != nullptr ? (starts[at] / width) + misplaced_
                                                    : std::get<std::size_t>(target),
                                  rows - 1);
        const auto first = [&starts, width](std::size_t value)
        {
            return nenenib::core::Column{
                static_cast<std::size_t>(
                    std::lower_bound(starts.begin(), std::prev(starts.end()), value * width) -
                    starts.begin()) +
                1};
        };
        return nenenib::application::TextRow{
            row, first(row), row + 1 < rows ? first(row + 1) : nenenib::core::Column{count + 1},
            rows};
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
    // 位置の問いに 1 つ後ろの段を答える（#664 の番兵の契約）。番号の問いは正しく答える。
    void misplace_rows() noexcept
    {
        misplaced_ = 1;
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

    // 表示文字列の各 code point が始まる升と、末尾に全体の升の数。升は行頭からの仮想桁に、行末の 1
    // 升 に入らず次の画面行へ送った全角の空きを足したもの。
    [[nodiscard]] static std::vector<std::size_t> cell_starts(std::string_view text,
                                                              std::size_t width)
    {
        std::vector<std::size_t> starts;
        std::size_t cell = 0;
        std::size_t column = 0;
        for (nenenib::core::Offset at{0}; at.value < text.size();
             at = nenenib::core::next_code_point(text, at))
        {
            const char32_t value = nenenib::core::code_point_at(text, at);
            const auto advance = nenenib::core::column_advance(value, column);
            const bool pushed = value != U'\t' && advance > 1 && advance <= width &&
                                (cell % width) + advance > width;
            cell += pushed ? width - (cell % width) : 0;
            starts.push_back(cell);
            cell += advance;
            column += advance;
        }
        starts.push_back(cell);
        return starts;
    }

    [[nodiscard]] static std::size_t columns(const nenenib::application::TextLayoutRequest &request)
    {
        // 巨大な行の窓は折り返さない（ADR 0139 の決定 2）。折り返さない幅は全体の升より広い。
        if (request.settings.wrapping == nenenib::core::LineWrapping::no_wrap ||
            request.geometry.width <= 0 || request.window.active)
        {
            return cell_starts(request.text, std::numeric_limits<std::size_t>::max()).back() + 1;
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
    std::size_t misplaced_ = 0;
};
} // namespace nenenib::tests
