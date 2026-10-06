// 字体選択の保持の段（ADR 0071 の決定 3・4）を、OS の役の替え玉で確かめる。
// 保持するのは鍵を作れた要求への OS の S_OK の答えだけで、E_FAIL と S_FALSE はそのまま返して
// 覚えない。保持は 128 件で、超えたら古いものから FIFO で入れ替わる。
#include "FontFallbackCache.hpp"
#include "ScriptedFontFallback.hpp"
#include "ScriptedTextSource.hpp"
#include "SourceFault.hpp"
#include "WindowChecks.hpp"

#include <string>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Cache = Microsoft::WRL::ComPtr<win32::FontFallbackCache>;
using Fallback = Microsoft::WRL::ComPtr<ScriptedFontFallback>;
using Source = Microsoft::WRL::ComPtr<ScriptedTextSource>;

constexpr UINT32 retained_entries = 128;

win32::FontFallbackRequest request_of(ScriptedTextSource *source)
{
    return {source,
            0,
            source->length(),
            nullptr,
            L"Cascadia Code",
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL};
}

// 1 回引いて、返った scale（OS の替え玉の何回目の答えか）を返す。
FLOAT map_once(IDWriteFontFallback &cache, const win32::FontFallbackRequest &request,
               HRESULT expected)
{
    UINT32 mapped = 0;
    Microsoft::WRL::ComPtr<IDWriteFont> font;
    FLOAT scale = 0;
    const HRESULT result = cache.MapCharacters(
        request.source, request.position, request.length, request.collection, request.family,
        request.weight, request.style, request.stretch, &mapped, font.GetAddressOf(), &scale);
    expect(result == expected, "the cache returns the HRESULT of the answer unchanged");
    expect(mapped == request.length && font == nullptr,
           "the mapped length and font are those of the answer");
    return scale;
}

// 1 回目は OS に聞き、2 回目は保持から同じ答えを返す。
void expect_retained(IDWriteFontFallback &cache, const ScriptedFontFallback &fallback,
                     const win32::FontFallbackRequest &request, const char *message)
{
    const UINT32 before = fallback.calls();
    const FLOAT first = map_once(cache, request, S_OK);
    const FLOAT second = map_once(cache, request, S_OK);
    expect(first == static_cast<FLOAT>(before + 1) && second == first &&
               fallback.calls() == before + 1,
           message);
}

// 2 回とも OS に聞き、OS の答えをそのまま返す。
void expect_delegated(IDWriteFontFallback &cache, const ScriptedFontFallback &fallback,
                      const win32::FontFallbackRequest &request, const char *message)
{
    const UINT32 before = fallback.calls();
    const FLOAT first = map_once(cache, request, fallback.result());
    const FLOAT second = map_once(cache, request, fallback.result());
    expect(first == static_cast<FLOAT>(before + 1) && second == static_cast<FLOAT>(before + 2) &&
               fallback.calls() == before + 2,
           message);
}

void verify_keys_reach_the_cache()
{
    const Fallback fallback = Microsoft::WRL::Make<ScriptedFontFallback>();
    const Cache cache = Microsoft::WRL::Make<win32::FontFallbackCache>(fallback);
    const Source source = Microsoft::WRL::Make<ScriptedTextSource>();
    auto request = request_of(source.Get());
    expect_retained(*cache.Get(), *fallback.Get(), request, "a complete request is retained");
    source->set_language(L"en-us");
    expect_retained(*cache.Get(), *fallback.Get(), request,
                    "a different language asks the OS again");
    request.weight = DWRITE_FONT_WEIGHT_BOLD;
    expect_retained(*cache.Get(), *fallback.Get(), request, "a different weight asks the OS again");
    source->set_text(L"A😀");
    request.length = source->length();
    expect_retained(*cache.Get(), *fallback.Get(), request, "a surrogate pair text is retained");
    source->set_text(L"A😁");
    expect_retained(*cache.Get(), *fallback.Get(), request,
                    "a text that differs in a low surrogate asks the OS again");
}

void verify_unretained_requests()
{
    const Fallback fallback = Microsoft::WRL::Make<ScriptedFontFallback>();
    const Cache cache = Microsoft::WRL::Make<win32::FontFallbackCache>(fallback);
    const Source source = Microsoft::WRL::Make<ScriptedTextSource>();
    const auto base = request_of(source.Get());
    auto changed = base;
    changed.position = 1;
    changed.length = base.length - 1;
    expect_delegated(*cache.Get(), *fallback.Get(), changed,
                     "a request not at position 0 is delegated every time");
    source->set_text(std::wstring(257, L'日'));
    changed = request_of(source.Get());
    expect_delegated(*cache.Get(), *fallback.Get(), changed,
                     "a request over 256 units is delegated every time");
    source->set_text(L"日本語");
    const std::wstring longer(129, L'f');
    changed = base;
    changed.family = longer.c_str();
    expect_delegated(*cache.Get(), *fallback.Get(), changed,
                     "a family name over 128 units is delegated every time");
    source->set_fault(SourceFault::broken_extension);
    expect_delegated(*cache.Get(), *fallback.Get(), base,
                     "an extended source is delegated every time");
    source->set_fault(SourceFault::none);
    expect_retained(*cache.Get(), *fallback.Get(), base,
                    "none of the delegated requests was retained");
}

void verify_os_failures()
{
    const Fallback fallback = Microsoft::WRL::Make<ScriptedFontFallback>();
    const Cache cache = Microsoft::WRL::Make<win32::FontFallbackCache>(fallback);
    const Source source = Microsoft::WRL::Make<ScriptedTextSource>();
    const auto request = request_of(source.Get());
    fallback->set_result(E_FAIL);
    expect_delegated(*cache.Get(), *fallback.Get(), request,
                     "an OS E_FAIL is returned as is and not retained");
    fallback->set_result(S_FALSE);
    expect_delegated(*cache.Get(), *fallback.Get(), request,
                     "an OS S_FALSE is returned as is and not retained");
    fallback->set_result(S_OK);
    expect_retained(*cache.Get(), *fallback.Get(), request, "only an OS S_OK is retained");
}

void fill_numbered(IDWriteFontFallback &cache, const ScriptedFontFallback &fallback,
                   ScriptedTextSource &source)
{
    for (UINT32 index = 0; index < retained_entries; ++index)
    {
        source.set_text(std::to_wstring(index));
        expect_retained(cache, fallback, request_of(&source), "a numbered text is retained");
    }
}

void verify_first_in_first_out()
{
    const Fallback fallback = Microsoft::WRL::Make<ScriptedFontFallback>();
    const Cache cache = Microsoft::WRL::Make<win32::FontFallbackCache>(fallback);
    const Source source = Microsoft::WRL::Make<ScriptedTextSource>();
    source->set_text(L"oldest");
    expect_retained(*cache.Get(), *fallback.Get(), request_of(source.Get()),
                    "the oldest text is retained");
    fill_numbered(*cache.Get(), *fallback.Get(), *source.Get());
    // 129 件目（"127"）が最も古い "oldest" と入れ替わった。
    source->set_text(L"oldest");
    expect_retained(*cache.Get(), *fallback.Get(), request_of(source.Get()),
                    "the oldest text was replaced by the 129th");
    // "oldest" を戻したので、次に古い "0" が入れ替わり、"1" はまだ残る。
    const UINT32 before = fallback->calls();
    source->set_text(L"1");
    static_cast<void>(map_once(*cache.Get(), request_of(source.Get()), S_OK));
    expect(fallback->calls() == before, "the second oldest text is still retained");
    source->set_text(L"0");
    static_cast<void>(map_once(*cache.Get(), request_of(source.Get()), S_OK));
    expect(fallback->calls() == before + 1, "the next oldest text was replaced first in first out");
}
} // namespace

void verify_fallback_cache()
{
    verify_keys_reach_the_cache();
    verify_unretained_requests();
    verify_os_failures();
    verify_first_in_first_out();
}
} // namespace nenenib::tests::ui
