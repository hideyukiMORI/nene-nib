// #900: Ctrl+P の面の行の文字組みの保持の契約（#924 のタブの題名も同じ型）。生成の数は替え玉の make
// が数える。 layout は本物の DirectWrite で作る（窓・device は作らない）。
#include "DirectWriteFactory.hpp"
#include "PaletteRowLayouts.hpp"
#include "WindowChecks.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests::ui
{
namespace
{
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using Layout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;
using nenenib::ui::win32::PaletteRowKey;
using nenenib::ui::win32::PaletteRowLayouts;
using nenenib::ui::win32::PaletteTextRole;
constexpr core::LayoutRect row_area{16, 40, 616, 72};
constexpr std::uint32_t dpi = 96;
constexpr std::array<std::string_view, 8> names{
    "alpha.txt", "beta.md", "gamma.cpp", "delta.hpp", "設定", "日本語.txt", "epsilon", "zeta.json"};

// 生成の数を数える替え玉。layout は 1 つを使い回す（中身は契約に関係しない）。
class CountingMake final
{
  public:
    explicit CountingMake(Layout layout) : layout_(std::move(layout)) {}
    [[nodiscard]] Layout make()
    {
        ++made_;
        return layout_;
    }
    [[nodiscard]] std::size_t take()
    {
        const auto made = made_;
        made_ = 0;
        return made;
    }

  private:
    Layout layout_;
    std::size_t made_ = 0;
};

[[nodiscard]] PaletteRowKey key_of(IDWriteTextFormat *format, std::string_view text,
                                   std::size_t row)
{
    const auto offset = static_cast<std::int32_t>(row) * 32;
    return {format, PaletteTextRole::plain, text,
            core::LayoutRect{row_area.left, row_area.top + offset, row_area.right,
                             row_area.bottom + offset},
            dpi};
}

void draw_rows(PaletteRowLayouts &layouts, CountingMake &counting, IDWriteTextFormat *format,
               const std::array<std::string_view, 8> &rows)
{
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        auto key = key_of(format, rows.at(index), index);
        expect(layouts.obtain(key, [&] { return counting.make(); }) != nullptr,
               "a palette row gets its layout");
        key.role = PaletteTextRole::trimmed;
        expect(layouts.obtain(key, [&] { return counting.make(); }) != nullptr,
               "a palette row gets its trimmed detail layout");
    }
}

void verify_frames(CountingMake &counting, IDWriteTextFormat *format)
{
    PaletteRowLayouts layouts;
    draw_rows(layouts, counting, format, names);
    expect(counting.take() == names.size() * 2, "the first frame creates every visible field");
    draw_rows(layouts, counting, format, names);
    expect(counting.take() == 0, "the same rows in the next frame create nothing");
    auto changed = names;
    changed.at(3) = "changed.txt";
    draw_rows(layouts, counting, format, changed);
    expect(counting.take() == 2, "a changed row creates only its own fields");
    // 打鍵で列が 1 つ上へずれても、文字列と寸法が同じなら原点は鍵ではない。
    for (std::size_t index = 1; index < names.size(); ++index)
    {
        (void)layouts.obtain(key_of(format, names.at(index), index - 1),
                             [&] { return counting.make(); });
    }
    expect(counting.take() == 0, "rows moved to another origin are reused");
    layouts.clear();
    draw_rows(layouts, counting, format, names);
    expect(counting.take() == names.size() * 2, "clearing (format/DPI/close) recreates all");
}

void verify_keys(CountingMake &counting, IDWriteTextFormat *format, IDWriteTextFormat *other)
{
    PaletteRowLayouts layouts;
    const auto make = [&] { return counting.make(); };
    std::string owned = "owned.txt";
    (void)layouts.obtain(key_of(format, owned, 0), make);
    owned = "different";
    (void)layouts.obtain(key_of(format, "owned.txt", 0), make);
    expect(counting.take() == 1, "the key owns the whole text");
    auto key = key_of(format, "owned.txt", 0);
    key.role = PaletteTextRole::trailing;
    (void)layouts.obtain(key, make);
    expect(counting.take() == 1, "a different role misses");
    key = key_of(other, "owned.txt", 0);
    (void)layouts.obtain(key, make);
    expect(counting.take() == 1, "a different format (family or size) misses");
    key = key_of(format, "owned.txt", 0);
    key.area.right += 1;
    (void)layouts.obtain(key, make);
    expect(counting.take() == 1, "a different width misses");
    key = key_of(format, "owned.txt", 0);
    key.dpi = 144;
    (void)layouts.obtain(key, make);
    expect(counting.take() == 1, "a different DPI misses");
    const auto failed = layouts.obtain(key_of(format, "failed", 0), [] { return Layout{}; });
    expect(failed == nullptr, "a failed creation is returned as null");
    (void)layouts.obtain(key_of(format, "failed", 0), make);
    expect(counting.take() == 1, "a failed creation is not retained and is retried");
    const auto before = layouts.size();
    key = key_of(format, "empty", 0);
    key.area.right = key.area.left;
    expect(layouts.obtain(key, make) == nullptr, "a zero-width field gets no layout");
    key = key_of(format, "empty", 0);
    key.area.bottom = key.area.top - 1;
    expect(layouts.obtain(key, make) == nullptr, "a negative-height field gets no layout");
    expect(counting.take() == 0, "a field without room creates no layout");
    expect(layouts.size() == before, "a field without room is not retained");
}

void verify_capacity(CountingMake &counting, IDWriteTextFormat *format)
{
    PaletteRowLayouts layouts;
    const auto make = [&] { return counting.make(); };
    constexpr auto capacity = PaletteRowLayouts::capacity;
    expect(capacity == core::palette_row_limit * 4, "the bound is four palettes of rows");
    std::array<std::string, capacity + 1> texts{};
    for (std::size_t index = 0; index < texts.size(); ++index)
    {
        texts.at(index) = "row-" + std::to_string(index);
    }
    for (std::size_t index = 0; index < capacity; ++index)
    {
        (void)layouts.obtain(key_of(format, texts.at(index), 0), make);
    }
    // 最も古い 0 番を使い直すと、捨てられるのは次に古い 1 番。
    (void)layouts.obtain(key_of(format, texts.at(0), 0), make);
    (void)layouts.obtain(key_of(format, texts.at(capacity), 0), make);
    expect(counting.take() == capacity + 1, "each distinct field is created once");
    expect(layouts.size() == capacity, "the retention never exceeds its bound");
    (void)layouts.obtain(key_of(format, texts.at(0), 0), make);
    expect(counting.take() == 0, "a recently used field survives eviction");
    (void)layouts.obtain(key_of(format, texts.at(1), 0), make);
    expect(counting.take() == 1, "the least recently used field is evicted first");
}

// タブの題名（#924）。欄は帯のタブの中で、× のあるタブは題名が狭い。色（hover）は鍵ではない。
[[nodiscard]] PaletteRowKey title_key(IDWriteTextFormat *format, std::string_view title,
                                      std::size_t index, std::size_t active)
{
    const auto left = 8 + (static_cast<std::int32_t>(index) * 160);
    const auto right = left + (index == active ? 120 : 144);
    return {format, PaletteTextRole::plain, title, core::LayoutRect{left, 0, right, 36}, dpi};
}

void verify_tab_titles(CountingMake &counting, IDWriteTextFormat *format)
{
    PaletteRowLayouts layouts;
    const auto make = [&] { return counting.make(); };
    const auto draw = [&](const std::array<std::string_view, 8> &titles, std::size_t active)
    {
        for (std::size_t index = 0; index < titles.size(); ++index)
        {
            const auto key = title_key(format, titles.at(index), index, active);
            expect(layouts.obtain(key, make) != nullptr, "a tab title gets its layout");
        }
    };
    expect(names.size() <= PaletteRowLayouts::capacity, "the visible tabs fit the bound");
    draw(names, 0);
    expect(counting.take() == names.size(), "the first frame creates every tab title");
    draw(names, 0);
    expect(counting.take() == 0, "the same titles in the next frame create nothing");
    auto changed = names;
    changed.at(2) = "● gamma.cpp";
    draw(changed, 0);
    expect(counting.take() == 1, "a changed title (the unsaved mark) creates only its own");
    draw(changed, 5);
    expect(counting.take() == 2, "switching tabs creates only the two titles whose width changed");
    draw(changed, 5);
    expect(counting.take() == 0, "the switched titles are then reused");
}

[[nodiscard]] Format make_format(IDWriteFactory2 &factory, float size)
{
    Format format;
    const auto made = factory.CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                               DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                               size, L"", &format);
    expect(SUCCEEDED(made), "the palette contract format is created");
    return format;
}
} // namespace

void verify_palette_layouts()
{
    const auto factory = directwrite_factory();
    if (!factory)
    {
        unmeasured("palette row contracts: DirectWrite unavailable");
        return;
    }
    const auto format = make_format(*factory.Get(), 12.0F);
    const auto other = make_format(*factory.Get(), 14.0F);
    if (!format || !other)
    {
        return;
    }
    Layout layout;
    const auto made = factory->CreateTextLayout(L"row", 3, format.Get(), 600.0F, 32.0F, &layout);
    expect(SUCCEEDED(made), "the contract layout is created");
    if (!layout)
    {
        return;
    }
    CountingMake counting(layout);
    verify_frames(counting, format.Get());
    verify_keys(counting, format.Get(), other.Get());
    verify_capacity(counting, format.Get());
    verify_tab_titles(counting, format.Get());
}
} // namespace nenenib::tests::ui
