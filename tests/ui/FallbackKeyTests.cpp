// 字体選択の保持の鍵（ADR 0071 の決定 2）を、替え玉の source で確かめる。
// 鍵が等しいのは、全文・言語・数字の置換・family・collection・向き・weight / style / stretch が
// すべて同じときだけ。完結していない要求と大きすぎる要求は鍵を作らない（保持しない）。
#include "DirectWriteFactory.hpp"
#include "FontFallbackKey.hpp"
#include "ScriptedTextSource.hpp"
#include "SourceFault.hpp"
#include "WindowChecks.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Key = std::optional<win32::FontFallbackKey>;
using Source = Microsoft::WRL::ComPtr<ScriptedTextSource>;

constexpr const wchar_t *family_name = L"Cascadia Code";

win32::FontFallbackRequest request_of(ScriptedTextSource *source, const wchar_t *family)
{
    return {source,
            0,
            source->length(),
            nullptr,
            family,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL};
}

bool same(const Key &left, const Key &right)
{
    return left.has_value() && right.has_value() && left.value() == right.value();
}

bool differ(const Key &left, const Key &right)
{
    return left.has_value() && right.has_value() && !(left.value() == right.value());
}

void verify_request_fields(IDWriteFontCollection *collection)
{
    const Source source = Microsoft::WRL::Make<ScriptedTextSource>();
    const auto base = request_of(source.Get(), family_name);
    const Key key = win32::fallback_key(base);
    expect(key.has_value() && key.value().text == L"日本語" && key.value().locale_name == L"ja-jp",
           "a complete source keys its whole text and its language");
    expect(same(key, win32::fallback_key(base)), "the same request gives an equal key");
    auto changed = base;
    changed.family = L"Consolas";
    expect(differ(key, win32::fallback_key(changed)), "a different family is a different key");
    changed = base;
    changed.weight = DWRITE_FONT_WEIGHT_BOLD;
    expect(differ(key, win32::fallback_key(changed)), "a different weight is a different key");
    changed = base;
    changed.style = DWRITE_FONT_STYLE_ITALIC;
    expect(differ(key, win32::fallback_key(changed)), "a different style is a different key");
    changed = base;
    changed.stretch = DWRITE_FONT_STRETCH_EXPANDED;
    expect(differ(key, win32::fallback_key(changed)), "a different stretch is a different key");
    changed = base;
    changed.collection = collection;
    const Key with_collection = win32::fallback_key(changed);
    expect(differ(key, with_collection), "a different collection object is a different key");
    expect(same(with_collection, win32::fallback_key(changed)),
           "the same collection object gives an equal key");
}

void verify_source_fields(IDWriteFactory2 &factory)
{
    const Source source = Microsoft::WRL::Make<ScriptedTextSource>();
    const auto request = request_of(source.Get(), family_name);
    const Key key = win32::fallback_key(request);
    source->set_text(L"日本人");
    expect(differ(key, win32::fallback_key(request)), "a different whole text is a different key");
    source->set_text(L"日本語");
    source->set_language(L"en-us");
    expect(differ(key, win32::fallback_key(request)), "a different language is a different key");
    source->set_language(L"ja-jp");
    source->set_direction(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    expect(differ(key, win32::fallback_key(request)), "a different direction is a different key");
    source->set_direction(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    Microsoft::WRL::ComPtr<IDWriteNumberSubstitution> first;
    Microsoft::WRL::ComPtr<IDWriteNumberSubstitution> second;
    expect(SUCCEEDED(factory.CreateNumberSubstitution(DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL,
                                                      L"ar-SA", TRUE, &first)) &&
               SUCCEEDED(factory.CreateNumberSubstitution(
                   DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA", TRUE, &second)),
           "two number substitutions with the same settings are made");
    source->set_substitution(first);
    const Key substituted = win32::fallback_key(request);
    expect(differ(key, substituted), "a number substitution is a different key");
    source->set_substitution(second);
    expect(differ(substituted, win32::fallback_key(request)),
           "another number substitution object with the same settings is a different key");
    expect(!same(key, Key{}), "an absent key never equals a key");
}

void expect_rejected(const win32::FontFallbackRequest &request, const char *message)
{
    expect(!win32::fallback_key(request).has_value(), message);
}

void verify_incomplete_requests()
{
    const Source source = Microsoft::WRL::Make<ScriptedTextSource>();
    const auto base = request_of(source.Get(), family_name);
    auto changed = base;
    changed.position = 1;
    changed.length = base.length - 1;
    expect_rejected(changed, "a request that does not start at position 0 is not retained");
    changed = base;
    changed.length = base.length - 1;
    expect_rejected(changed, "a request shorter than the source text is not retained");
    changed = base;
    changed.length = 0;
    expect_rejected(changed, "an empty request is not retained");
    changed = base;
    changed.family = nullptr;
    expect_rejected(changed, "a request without a family name is not retained");
    source->set_property_lengths(base.length - 1, UINT32_MAX);
    expect_rejected(base, "a language that covers only part of the text is not retained");
    source->set_property_lengths(UINT32_MAX, base.length - 1);
    expect_rejected(base, "a number substitution that covers only part is not retained");
    source->set_property_lengths(UINT32_MAX, UINT32_MAX);
    source->set_fault(SourceFault::hidden_tail);
    expect_rejected(base, "a source with text after the request is not retained");
    source->set_fault(SourceFault::hidden_prefix);
    expect_rejected(base, "a source with text before position 0 is not retained");
    source->set_fault(SourceFault::broken_text);
    expect_rejected(base, "a source whose text cannot be read is not retained");
    source->set_fault(SourceFault::broken_extension);
    expect_rejected(base, "an extended source (QueryInterface not E_NOINTERFACE) is not retained");
    source->set_fault(SourceFault::none);
    expect(win32::fallback_key(base).has_value(), "the restored source is retained again");
}

void verify_size_bounds()
{
    const Source source = Microsoft::WRL::Make<ScriptedTextSource>();
    source->set_text(std::wstring(256, L'日'));
    expect(win32::fallback_key(request_of(source.Get(), family_name)).has_value(),
           "256 UTF-16 units are retained");
    source->set_text(std::wstring(257, L'日'));
    expect_rejected(request_of(source.Get(), family_name), "257 UTF-16 units are not retained");
    source->set_text(L"日本語");
    const std::wstring longest(128, L'f');
    const std::wstring longer(129, L'f');
    expect(win32::fallback_key(request_of(source.Get(), longest.c_str())).has_value(),
           "a family name of 128 units is retained");
    expect_rejected(request_of(source.Get(), longer.c_str()),
                    "a family name over 128 units is not retained");
    source->set_language(longest);
    expect(win32::fallback_key(request_of(source.Get(), family_name)).has_value(),
           "a language name of 128 units is retained");
    source->set_language(longer);
    expect_rejected(request_of(source.Get(), family_name),
                    "a language name over 128 units is not retained");
}

void verify_surrogates()
{
    const Source source = Microsoft::WRL::Make<ScriptedTextSource>();
    source->set_text(L"A😀");
    const auto request = request_of(source.Get(), family_name);
    const Key smile = win32::fallback_key(request);
    expect(smile.has_value() && request.length == 3, "a surrogate pair is two units of the key");
    source->set_text(L"A😁");
    expect(differ(smile, win32::fallback_key(request)),
           "texts that differ only in a low surrogate are different keys");
    std::wstring pairs;
    for (int index = 0; index < 128; ++index)
    {
        pairs += L"😀";
    }
    source->set_text(pairs);
    expect(win32::fallback_key(request_of(source.Get(), family_name)).has_value(),
           "128 surrogate pairs (256 units) are retained");
    source->set_text(pairs + L"😀");
    expect_rejected(request_of(source.Get(), family_name),
                    "129 surrogate pairs (258 units) are not retained");
}
} // namespace

void verify_fallback_keys()
{
    const auto factory = directwrite_factory();
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    expect(factory != nullptr &&
               SUCCEEDED(factory->GetSystemFontCollection(collection.GetAddressOf(), FALSE)),
           "the DirectWrite factory and its system collection are made");
    if (factory == nullptr || collection == nullptr)
    {
        return;
    }
    verify_request_fields(collection.Get());
    verify_source_fields(*factory.Get());
    verify_incomplete_requests();
    verify_size_bounds();
    verify_surrogates();
}
} // namespace nenenib::tests::ui
