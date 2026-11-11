// #1455（ADR 0172 の決定 5・#1018 の段 2）: 起動画面の画素。WIC の bitmap（SoftwareTarget）に
// StartScreenPainter で描き、題と節の見出しの墨・選んだ行の地（current_line）・選んでいない行の地
// （本文の background）・鍵の枠の面（panel）・名前と場所（場所は頭側を切って列の右の余白の手前で
// 終わる）・「はじめる」の補足は名前のすぐ後ろ・ショートカットの鍵の枠は項目の左端に揃うこと・行の
// 文字組みの保持（最初の frame の数と、次の frame で増えないこと）を、ダークとライトで確かめる
// （追補 2026-11-11 の 20〜22）。
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
// 最初の frame（履歴とブックマークを読む前）の文字組みの数: 題 1・見出し 2（はじめる・
// ショートカット）・「はじめる」の名前 3 と補足 2 と鍵の枠 2（新しいファイル・ファイルを開く・
// #1480）・ショートカットの枠 3 と説明 3。
constexpr std::size_t first_frame_layouts = 16;
// 最近 2・ブックマーク 1 を読み、最初の行を選んだ frame: 見出し 2・印 1・「…」1 と、パスの行ごとに
// 名前・場所の全文（幅を測る）・描いた場所の 3 つが増える。
constexpr std::size_t listed_layouts = first_frame_layouts + 2 + 1 + 1 + (3 * 3);

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

// 書式 5 つ（題・行の字・見出し・鍵・鍵の説明）。
using Formats = std::vector<Format>;

// 行の字の書式は、製品（エクスプローラーの面の書式）と同じく書式に文字単位の省略の印を持つ
// （レビュー M2 の条件）。
[[nodiscard]] Format trimmed_format_of(IDWriteFactory2 &factory)
{
    auto format = format_of(factory, L"Segoe UI", 12.0F, DWRITE_TEXT_ALIGNMENT_LEADING);
    Microsoft::WRL::ComPtr<IDWriteInlineObject> sign;
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    if (format == nullptr ||
        FAILED(factory.CreateEllipsisTrimmingSign(format.Get(), sign.GetAddressOf())) ||
        FAILED(format->SetTrimming(&trimming, sign.Get())))
    {
        return nullptr;
    }
    return format;
}

[[nodiscard]] Formats formats_of(IDWriteFactory2 &factory)
{
    return {format_of(factory, L"Segoe UI", 22.5F, DWRITE_TEXT_ALIGNMENT_LEADING),
            trimmed_format_of(factory),
            format_of(factory, L"Segoe UI", 12.0F, DWRITE_TEXT_ALIGNMENT_LEADING),
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
        KeycapStyle{9.0F, 20.0F, 5.0F, palette.panel, palette.panel_border, palette.text},
        "Ctrl+T",
        "Ctrl+O",
        "",
        dpi};
}

[[nodiscard]] core::BodyLayout body_of()
{
    return core::body_layout_in(core::LayoutRect{0, 0, 1300, 900}, dpi, core::default_font_size());
}

// 最近 2（2 つ目は列に収まらない長い場所と短い名前）・ブックマーク 1・最初の行を選んだ画面。
[[nodiscard]] application::StartScreenView listed_view()
{
    const std::vector<core::FilePath> recent{
        core::FilePath::parse("C:\\work\\notes.txt").value(),
        core::FilePath::parse("C:\\" + std::string(120, 'w') + "\\notes2.txt").value()};
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
    // 長い場所の行は、項目の左端に名前の墨があり、頭を切った場所が項目の欄の右端まで続き、行の
    // 右の余白には墨が無い。
    const auto &label = layout.rows.at(1).label;
    // 見出しと 2 行は bitmap の高さに収まらないので、長い場所の行だけを別に描く。
    const auto path_top = second.top - 20;
    const auto path = paint(view, layout, path_top);
    expect(
        ink_in(path,
               shifted(core::LayoutRect{label.left, label.top, label.left + 20, label.bottom},
                       path_top),
               palette.background) > 0 &&
            ink_in(path,
                   shifted(core::LayoutRect{label.right - 40, label.top, label.right, label.bottom},
                           path_top),
                   palette.background) > 0 &&
            ink_in(path,
                   shifted(core::LayoutRect{label.right, label.top, second.right, label.bottom},
                           path_top),
                   palette.background) == 0,
        "a long folder is cut at its head and ends before the right padding");
}

// 「はじめる」の補足は名前のすぐ後ろ。鍵のある「新しいファイル」と「ファイルを開く」は右端に
// （#1480）、ショートカットは項目の左端に、本文の案内と同じ鍵の枠（面は panel）。鍵の無い「フォルダ
// を開く」の右端は空く。
void verify_keycaps(const StripPainter &paint, const core::StartScreenLayout &layout,
                    const core::Palette &palette)
{
    const auto view = listed_view();
    const auto &fresh = layout.rows.at(3).label;
    const auto &open = layout.rows.at(4).label;
    const auto open_top = fresh.top - 10;
    const auto opened = paint(view, layout, open_top);
    expect(ink_in(opened,
                  shifted(
                      core::LayoutRect{fresh.left + 120, fresh.top, fresh.left + 200, fresh.bottom},
                      open_top),
                  palette.background) > 0 &&
               pixel_is(opened, fresh.right - 3, ((fresh.top + fresh.bottom) / 2) - open_top,
                        palette.panel),
           "the new-file note follows its name and its key sits in a keycap at the right end");
    const auto &folder = layout.rows.at(5).label;
    const auto folder_top = folder.top - 10;
    const auto folders = paint(view, layout, folder_top);
    expect(
        ink_in(folders,
               shifted(core::LayoutRect{folder.right - 60, folder.top, folder.right, folder.bottom},
                       folder_top),
               palette.background) == 0,
        "the open-folder row without a key leaves the right end empty");
    expect(
        pixel_is(opened, open.right - 3, ((open.top + open.bottom) / 2) - open_top, palette.panel),
        "the open-file row shows its key in a keycap at the right end");
    const auto &key = layout.keys.at(1);
    const auto key_top = key.key.top - 30;
    const auto keys = paint(view, layout, key_top);
    const auto key_middle = ((key.key.top + key.key.bottom) / 2) - key_top;
    expect(pixel_is(keys, key.key.left + 3, key_middle, palette.panel) &&
               pixel_is(keys, key.key.right - 3, key_middle, palette.background),
           "a short shortcut keycap starts at the left axis of the rows");
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
// 場所の頭側の切り方（レビュー M1 / M2）。頭と尻で違う字の場所を、欄に入るように頭側を「…」で
// 切ると、先頭が「…」で尻（`\tail`）が残る。尻を切る実装なら尻が消えて落ちる。測る欄（8192 物理
// 画素）を超える長さでも尻が残る。入る場所は全文のまま。
void verify_head_trimming(SoftwareTarget &target, IDWriteFactory2 &factory, const Formats &formats)
{
    const auto palette = core::palette_for(core::Appearance::dark);
    const auto text = text_of(factory, formats, palette);
    PaletteRowLayouts layouts(StartScreenPainter::retained);
    const core::LayoutRect area{0, 0, 200, 24};
    const std::string tail = "\\tail";
    const std::string longer = "C:\\head" + std::string(100, 'm') + tail;
    const std::string longest = "C:\\head" + std::string(2000, 'm') + tail;
    const std::string fitting = "C:\\head\\tail";
    std::string cut;
    std::string cut_longest;
    std::string kept;
    static_cast<void>(target.painted(
        [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
        {
            StartScreenPainter painter(surface, *brush, text, layouts);
            cut = painter.head_trimmed(longer, area);
            cut_longest = painter.head_trimmed(longest, area);
            kept = painter.head_trimmed(fitting, area);
        }));
    const std::string ellipsis = "…";
    expect(cut.starts_with(ellipsis) && cut.ends_with(tail) && cut.size() < longer.size() &&
               cut.find("head") == std::string::npos,
           "a long folder loses its head behind an ellipsis and keeps its tail");
    expect(cut_longest.starts_with(ellipsis) && cut_longest.ends_with(tail) &&
               cut_longest.size() < cut.size() + 40,
           "a folder wider than the measuring box still keeps its tail");
    expect(kept == fitting, "a folder that fits is kept whole");
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
    verify_head_trimming(target, *factory.Get(), formats);
}
} // namespace nenenib::tests::ui
