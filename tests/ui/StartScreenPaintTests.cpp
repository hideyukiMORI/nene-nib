// #1455（ADR 0172 の決定 5・#1018 の段 2）: 起動画面の画素。WIC の bitmap（SoftwareTarget）に
// StartScreenPainter で描き、題と節の見出しの墨・選んだ行の地（current_line）・選んでいない行の地
// （本文の background）・鍵の枠の面（panel）・長いパスが列の右の余白の手前で切れること・行の
// 文字組みの保持（最初の frame の数と、次の frame で増えないこと）を、ダークとライトで確かめる。
// 配置は製品と同じ core::start_screen_layout。bitmap は 80 画素の高さなので、平行移動で見たい行を
// bitmap に入れて描く。窓・device は作らない。字の画素は字体に依るので、Segoe UI が無い機械では
// 測らない（QLT-013）。
#include "BodyLayout.hpp"
#include "DirectWriteFactory.hpp"
#include "EditMode.hpp"
#include "FilePath.hpp"
#include "FontSize.hpp"
#include "KeycapStyle.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "SoftwareTarget.hpp"
#include "StartScreen.hpp"
#include "StartScreenLayout.hpp"
#include "StartScreenPainter.hpp"
#include "StartScreenShape.hpp"
#include "StartScreenText.hpp"
#include "StartScreenView.hpp"
#include "WindowChecks.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace application = nenenib::application;
using nenenib::ui::win32::KeycapStyle;
using nenenib::ui::win32::PaletteRowLayouts;
using nenenib::ui::win32::StartScreenPainter;
using nenenib::ui::win32::StartScreenText;
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;

constexpr std::uint32_t dpi = 96;
// 最初の frame（履歴とブックマークを読む前）の文字組みの数: 題 1・見出し 2（はじめる・鍵）・
// 「はじめる」の名前 3 と補足 2 と鍵の枠 1・鍵の案内の枠 3 と説明 3。
constexpr std::size_t first_frame_layouts = 15;
// 最近 2・ブックマーク 1 を読み、最初の行を選んだ frame: 見出しが 2 つ・名前が 3 つ・印が 1
// つ増える。
constexpr std::size_t listed_layouts = first_frame_layouts + 2 + 3 + 1;

// 試験の書式（製品の renderer と同じ並び: 段落の中央・折り返さない）。
[[nodiscard]] Format format_of(IDWriteFactory2 &factory, const wchar_t *family, float size,
                               DWRITE_TEXT_ALIGNMENT alignment)
{
    Format format;
    if (FAILED(factory.CreateTextFormat(family, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size,
                                        L"", format.GetAddressOf())))
    {
        return nullptr;
    }
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    format->SetTextAlignment(alignment);
    return format;
}

// 書式 6 つ（題・項目・見出し・補足・鍵・鍵の説明）。
using Formats = std::vector<Format>;

[[nodiscard]] Formats formats_of(IDWriteFactory2 &factory)
{
    return {format_of(factory, L"Segoe UI", 22.5F, DWRITE_TEXT_ALIGNMENT_LEADING),
            format_of(factory, L"Consolas", 15.0F, DWRITE_TEXT_ALIGNMENT_LEADING),
            format_of(factory, L"Segoe UI", 12.0F, DWRITE_TEXT_ALIGNMENT_LEADING),
            format_of(factory, L"Segoe UI", 12.0F, DWRITE_TEXT_ALIGNMENT_TRAILING),
            format_of(factory, L"Consolas", 12.5F, DWRITE_TEXT_ALIGNMENT_LEADING),
            format_of(factory, L"Segoe UI", 13.5F, DWRITE_TEXT_ALIGNMENT_LEADING)};
}

[[nodiscard]] StartScreenText text_of(IDWriteFactory2 &factory, const Formats &formats,
                                      const core::Palette &palette)
{
    return StartScreenText{
        &factory,
        formats.at(0).Get(),
        formats.at(1).Get(),
        formats.at(2).Get(),
        formats.at(3).Get(),
        formats.at(4).Get(),
        formats.at(5).Get(),
        KeycapStyle{9.0F, 20.0F, 5.0F, palette.panel, palette.panel_border, palette.text},
        "Ctrl+O",
        dpi};
}

[[nodiscard]] core::BodyLayout body_of()
{
    return core::body_layout_in(core::LayoutRect{0, 0, 1300, 900}, dpi, core::default_font_size());
}

// 最近 2（2 つ目は列に収まらない長いパス）・ブックマーク 1・最初の行を選んだ画面。
[[nodiscard]] application::StartScreenView listed_view()
{
    const std::vector<core::FilePath> recent{
        core::FilePath::parse("C:\\work\\notes.txt").value(),
        core::FilePath::parse("C:\\work\\" + std::string(120, 'w') + ".txt").value()};
    const std::vector<core::FilePath> bookmarks{
        core::FilePath::parse("D:\\NeNeNib\\README.md").value()};
    return application::StartScreenView{core::start_entries(recent, bookmarks), std::size_t{0},
                                        true, core::start_key_entries(core::EditMode::ordinary)};
}

[[nodiscard]] application::StartScreenView first_view()
{
    return application::StartScreenView{core::start_entries({}, {}), std::nullopt, false,
                                        core::start_key_entries(core::EditMode::ordinary)};
}

// 画素（BGRA・premultiplied・不透明）が色と同じか。
[[nodiscard]] bool pixel_is(const SoftwareTarget::Pixels &pixels, std::int32_t x, std::int32_t y,
                            core::RgbColor color)
{
    const auto at =
        ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) * 4;
    return pixels.size() > at + 3 && pixels.at(at) == color.blue &&
           pixels.at(at + 1) == color.green && pixels.at(at + 2) == color.red;
}

// 矩形の中で地の色と違う画素（字の墨）の数。
[[nodiscard]] std::size_t ink_in(const SoftwareTarget::Pixels &pixels, const core::LayoutRect &area,
                                 core::RgbColor ground)
{
    std::size_t ink = 0;
    for (auto y = area.top; y < area.bottom; ++y)
    {
        for (auto x = area.left; x < area.right; ++x)
        {
            ink += pixel_is(pixels, x, y, ground) ? 0 : 1;
        }
    }
    return ink;
}

// 上端 top を bitmap の上端へ平行移動して描いた矩形（bitmap の座標）。
[[nodiscard]] core::LayoutRect shifted(const core::LayoutRect &area, std::int32_t top)
{
    return core::LayoutRect{area.left, area.top - top, area.right, area.bottom - top};
}

// 本文の地を塗ってから画面を描く口。top は bitmap の上端に置く画面の y。
class StripPainter final
{
  public:
    StripPainter(SoftwareTarget &target, const StartScreenText &text, PaletteRowLayouts &layouts,
                 const core::Palette &palette)
        : target_(target), text_(text), layouts_(layouts), palette_(palette)
    {
    }
    [[nodiscard]] SoftwareTarget::Pixels operator()(const application::StartScreenView &view,
                                                    const core::StartScreenLayout &layout,
                                                    std::int32_t top) const
    {
        const auto body = body_of();
        return target_.painted(
            [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
            {
                surface.SetTransform(D2D1::Matrix3x2F::Translation(0.0F, -static_cast<float>(top)));
                brush->SetColor(D2D1::ColorF(static_cast<float>(palette_.background.red) / 255.0F,
                                             static_cast<float>(palette_.background.green) / 255.0F,
                                             static_cast<float>(palette_.background.blue) / 255.0F,
                                             1.0F));
                surface.FillRectangle(D2D1::RectF(static_cast<float>(body.band.left),
                                                  static_cast<float>(body.band.top),
                                                  static_cast<float>(body.band.right),
                                                  static_cast<float>(body.band.bottom)),
                                      brush);
                StartScreenPainter painter(surface, *brush, text_, layouts_);
                painter.paint(view, layout, palette_, body.band);
                surface.SetTransform(D2D1::Matrix3x2F::Identity());
            });
    }

  private:
    SoftwareTarget &target_;
    StartScreenText text_;
    PaletteRowLayouts &layouts_;
    core::Palette palette_;
};

// 題と見出しは muted の墨、選んだ行は列の全幅が current_line、選んでいない行は本文の地。
void verify_rows(const StripPainter &paint, const core::StartScreenLayout &layout,
                 const core::Palette &palette)
{
    const auto view = listed_view();
    const auto title_top = layout.title.top - 4;
    const auto title = paint(view, layout, title_top);
    expect(ink_in(title, shifted(layout.title, title_top), palette.background) > 0,
           "the title is drawn");
    const auto &first = layout.rows.at(0).line;
    const auto row_top = first.top - 24;
    const auto rows = paint(view, layout, row_top);
    const auto middle = ((first.top + first.bottom) / 2) - row_top;
    expect(pixel_is(rows, first.left + 1, middle, palette.current_line) &&
               pixel_is(rows, first.right - 2, middle, palette.current_line),
           "the selected row is current_line across the column");
    expect(ink_in(rows, shifted(layout.rows.at(0).mark, row_top), palette.current_line) > 0,
           "the selected row carries its marker");
    const auto &second = layout.rows.at(1).line;
    expect(pixel_is(rows, second.right - 2, ((second.top + second.bottom) / 2) - row_top,
                    palette.background),
           "an unselected row keeps the body ground");
    const auto heading = layout.recent_heading.value_or(core::LayoutRect{});
    expect(layout.recent_heading.has_value() &&
               ink_in(rows, shifted(heading, row_top), palette.background) > 0,
           "the recent heading is drawn");
    // 長いパスは項目の欄の右端まで墨があり、行の右の余白には墨が無い。
    const auto &label = layout.rows.at(1).label;
    // 見出しと 2 行は bitmap の高さに収まらないので、長いパスの行だけを別に描く。
    const auto path_top = second.top - 20;
    const auto path = paint(view, layout, path_top);
    expect(ink_in(path,
                  shifted(core::LayoutRect{label.right - 40, label.top, label.right, label.bottom},
                          path_top),
                  palette.background) > 0 &&
               ink_in(path,
                      shifted(core::LayoutRect{label.right, label.top, second.right, label.bottom},
                              path_top),
                      palette.background) == 0,
           "a long path is cut before the right padding of the row");
}

// 「ファイルを開く」の補足と鍵の案内の枠は、本文の案内と同じ鍵の枠（面は panel）。
void verify_keycaps(const StripPainter &paint, const core::StartScreenLayout &layout,
                    const core::Palette &palette)
{
    const auto view = listed_view();
    const auto &open = layout.rows.at(4);
    const auto open_top = open.line.top - 30;
    const auto opened = paint(view, layout, open_top);
    expect(pixel_is(opened, open.side.right - 3,
                    ((open.side.top + open.side.bottom) / 2) - open_top, palette.panel),
           "the open-file row shows its key in a keycap");
    const auto &key = layout.keys.at(0);
    const auto key_top = key.key.top - 30;
    const auto keys = paint(view, layout, key_top);
    expect(pixel_is(keys, key.key.right - 3, ((key.key.top + key.key.bottom) / 2) - key_top,
                    palette.panel),
           "the key rows draw the guide keycap");
    expect(ink_in(keys, shifted(key.label, key_top), palette.background) > 0,
           "the key rows carry their names");
}

void verify_palette(SoftwareTarget &target, IDWriteFactory2 &factory, const Formats &formats,
                    const core::Palette &palette)
{
    const auto body = body_of();
    const auto text = text_of(factory, formats, palette);
    PaletteRowLayouts layouts(StartScreenPainter::retained);
    const StripPainter paint(target, text, layouts, palette);
    const auto first = first_view();
    const auto first_layout = core::start_screen_layout(
        body, dpi, core::start_screen_shape(first.entries, first.lists_read), 90);
    expect(first_layout.has_value(), "the first frame places the start screen");
    if (!first_layout.has_value())
    {
        return;
    }
    static_cast<void>(paint(first, first_layout.value(), 0));
    expect(layouts.size() == first_frame_layouts,
           "the first frame creates the fixed layouts of the start screen once");
    const auto view = listed_view();
    const auto placed = core::start_screen_layout(
        body, dpi, core::start_screen_shape(view.entries, view.lists_read), 90);
    if (!placed.has_value())
    {
        expect(false, "the listed frame places the start screen");
        return;
    }
    verify_rows(paint, placed.value(), palette);
    verify_keycaps(paint, placed.value(), palette);
    expect(layouts.size() == listed_layouts,
           "the read lists add only their own layouts and repaints create none");
}
} // namespace

void verify_start_screen_paint()
{
    SoftwareTarget target;
    const auto factory = directwrite_factory();
    if (!target.ready() || factory == nullptr)
    {
        unmeasured("the start screen needs WIC and DirectWrite");
        return;
    }
    if (!installed(*factory.Get(), L"Segoe UI") || !installed(*factory.Get(), L"Consolas"))
    {
        unmeasured("the start screen pixels need Segoe UI and Consolas");
        return;
    }
    const auto formats = formats_of(*factory.Get());
    for (const auto &format : formats)
    {
        if (format == nullptr)
        {
            unmeasured("the start screen formats could not be made");
            return;
        }
    }
    verify_palette(target, *factory.Get(), formats, core::palette_for(core::Appearance::dark));
    verify_palette(target, *factory.Get(), formats, core::palette_for(core::Appearance::light));
}
} // namespace nenenib::tests::ui
