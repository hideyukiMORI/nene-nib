#pragma once

#include "BodyLayout.hpp"
#include "BodyTextLayouts.hpp"
#include "EditorSettings.hpp"
#include "LayoutRect.hpp"
#include "PrintFailure.hpp"
#include "PrintJob.hpp"
#include "PrintLine.hpp"
#include "PrintPage.hpp"
#include "PrintPageInput.hpp"
#include "PrintPalette.hpp"
#include "PrintPaper.hpp"

#include <cstddef>
#include <d2d1.h>
#include <dwrite.h>
#include <expected>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 1 回の印刷の紙面（ADR 0177 の決定 3〜5）。本文を行に割って display_line と構文の色を写し、
// 本文と同じ書式（BodyTextStyle・紙の幅で文字単位に折り返す）の使い捨ての BodyTextLayouts で
// 行ごとの画面行の数を数え、core の print_rows_per_page と print_pages でページに割る。画面の
// 保持（合成ルートの BodyTextLayouts）には触れない。座標は DIP（dpi 96）。
class PrintComposition final
{
  public:
    [[nodiscard]] static std::expected<PrintComposition, application::PrintFailure>
    compose(const application::PrintJob &job, PrintPaper paper);
    [[nodiscard]] std::size_t page_count() const noexcept;
    // index 番目（0 始まり）のページを描く入力。この値が生きている間だけ有効。
    [[nodiscard]] PrintPageInput page(std::size_t index);
    [[nodiscard]] D2D1_SIZE_F paper_size() const noexcept;
    [[nodiscard]] const core::BodyLayout &body() const noexcept;
    [[nodiscard]] std::size_t rows_per_page() const noexcept;

  private:
    PrintComposition(core::EditorSettings settings, PrintPaper paper, bool gutter);
    [[nodiscard]] bool make_gutter_format();
    [[nodiscard]] bool add_lines(const application::PrintJob &job);

    core::EditorSettings settings_;
    PrintPaper paper_;
    core::PrintPalette palette_;
    core::LayoutRect header_{};
    core::BodyLayout body_{};
    std::string title_;
    std::vector<PrintLine> lines_;
    std::vector<core::PrintPage> pages_;
    std::size_t rows_per_page_ = 1;
    std::unique_ptr<BodyTextLayouts> layouts_;
    Microsoft::WRL::ComPtr<IDWriteFactory> factory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> gutter_;
};
} // namespace nenenib::ui::win32
