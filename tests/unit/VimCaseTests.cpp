// scope `--vim-case` の単体テスト（ADR 0042 決定 2・ADR 0015 決定 10 の追記・Issue #781）。
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "VimCase.hpp"

#include "../vim/VimFixtures.hpp"

#include <cstddef>

namespace nenenib::tests
{
namespace
{
using core::vim_folded;
using core::vim_to_lower;
using core::vim_to_upper;
using core::vim_toggled;

constexpr char32_t last_code_point = U'\x10FFFF';

// 表は Unicode 15.0 の簡単な写像（Python 3.12 の unicodedata・eng/case-mapping.py）。1 行ずつの例。
void verify_case_examples()
{
    expect(vim_to_upper(U'a') == U'A' && vim_to_lower(U'A') == U'a', "ASCII letters map");
    expect(vim_to_upper(U'1') == U'1' && vim_to_lower(U'1') == U'1', "digits keep themselves");
    expect(vim_to_upper(U'é') == U'É' && vim_to_lower(U'É') == U'é', "Latin-1 letters map");
    expect(vim_to_upper(U'ａ') == U'Ａ' && vim_to_lower(U'Ａ') == U'ａ', "full-width letters map");
    expect(vim_to_upper(U'α') == U'Α' && vim_to_lower(U'Ω') == U'ω', "Greek letters map");
    expect(vim_to_upper(U'п') == U'П' && vim_to_lower(U'Р') == U'р', "Cyrillic letters map");
    expect(vim_to_upper(U'\x101') == U'\x100' && vim_to_lower(U'\x100') == U'\x101' &&
               vim_to_upper(U'\x102') == U'\x102',
           "a range stepping by two maps only its own members");
    expect(vim_to_upper(U'ÿ') == U'Ÿ', "a far delta maps");
    expect(vim_to_upper(U'\x1E922') == U'\x1E900' && vim_to_lower(U'\x1E900') == U'\x1E922',
           "letters outside the BMP map");
}

// 写像を使わない code point（1 対多・ロケール・字でないもの・符号空間の端）。`ß` → `SS` は表の
// 外（cased_text）で、ここは `ß` を変えない。
void verify_case_kept()
{
    expect(vim_to_upper(U'ß') == U'ß' && vim_to_lower(U'\x1E9E') == U'ß',
           "a one-to-many mapping is not used; the simple lower of the capital sharp s is");
    expect(vim_to_upper(U'ı') == U'I' && vim_to_upper(U'i') == U'I',
           "the Turkish i keeps the locale-free mapping");
    expect(vim_to_upper(U'あ') == U'あ' && vim_to_lower(U'あ') == U'あ', "uncased letters stay");
    expect(vim_to_upper(U'\0') == U'\0' && vim_to_upper(last_code_point) == last_code_point,
           "the ends of the code space stay");
}

// UnicodeData の簡単な写像で上書きした 28 字（Issue #990・固定 Vim 9.1 の toupper() / tolower()）。
void verify_case_overrides()
{
    expect(vim_to_lower(U'\x130') == U'i' && vim_toggled(U'\x130') == U'i',
           "the dotted capital I lowers to i by the simple mapping");
    expect(vim_to_upper(U'\x1F80') == U'\x1F88' && vim_to_upper(U'\x1FA7') == U'\x1FAF' &&
               vim_to_upper(U'\x1FB3') == U'\x1FBC' && vim_to_upper(U'\x1FC3') == U'\x1FCC' &&
               vim_to_upper(U'\x1FF3') == U'\x1FFC',
           "a Greek small letter with ypogegrammeni uppers to its title case capital");
    expect(vim_toggled(U'\x1F88') == U'\x1F80' && vim_to_lower(U'\x1F80') == U'\x1F80',
           "the title case capital toggles back and the small letter does not lower");
    expect(vim_folded(U'\x1F80') == U'\x1F80' && vim_folded(U'\x1F88') == U'\x1F80' &&
               vim_folded(U'\x130') == U'\x130',
           "the folded forms keep what they were");
}

// タイトルケースの字は小文字として扱う（Vim の swapchar・固定 Vim 9.1 の実測）。
void verify_title_case()
{
    expect(vim_to_upper(U'ǅ') == U'Ǆ', "upper case turns a title case letter into the capital");
    expect(vim_to_lower(U'ǅ') == U'ǅ', "lower case keeps a title case letter, as Vim's swapchar");
    expect(vim_toggled(U'ǅ') == U'Ǆ', "the toggle treats a title case letter as lower case");
    expect(vim_toggled(U'a') == U'A' && vim_toggled(U'A') == U'a' && vim_toggled(U'1') == U'1',
           "the toggle swaps lower and upper and keeps the rest");
}

// 全 code point を歩いて、写像の数が生成器の数えた数と同じ（表の畳み方が写像を落とさない）。
void verify_case_counts()
{
    std::size_t uppered = 0;
    std::size_t lowered = 0;
    std::size_t toggled = 0;
    for (char32_t value = 0; value <= last_code_point; ++value)
    {
        uppered += vim_to_upper(value) != value ? 1U : 0U;
        lowered += vim_to_lower(value) != value ? 1U : 0U;
        toggled += vim_toggled(value) != value ? 1U : 0U;
    }
    expect(uppered == 1450, "1450 code points have a simple upper case in Unicode 15.0");
    expect(lowered == 1429, "1429 code points that are not lower case have a simple lower case");
    expect(toggled == 2879, "the toggle changes every code point that has either mapping");
}

void verify_unicode_case_fixtures()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("unicode-case-") || fixture.name.starts_with("case-rest-"))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 29, "the scope replays the 15 unicode-case and 14 case-rest fixtures");
}
} // namespace

void verify_vim_case_contracts()
{
    verify_case_examples();
    verify_case_kept();
    verify_case_overrides();
    verify_title_case();
    verify_case_counts();
}

void verify_vim_case_scope()
{
    verify_unicode_case_fixtures();
    verify_vim_case_contracts();
}
} // namespace nenenib::tests
