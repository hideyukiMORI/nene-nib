#include "SystemFontFamilies.hpp"

#include "Utf16.hpp"

#include <dwrite.h>
#include <wrl/client.h>

#include <string>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
using Names = Microsoft::WRL::ComPtr<IDWriteLocalizedStrings>;

// family の名前の 1 つ（en-us、無ければ最初）。読めなければ空。
[[nodiscard]] std::string family_name(IDWriteFontCollection &collection, UINT32 index)
{
    Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
    Names names;
    UINT32 slot = 0;
    BOOL found = FALSE;
    UINT32 length = 0;
    if (FAILED(collection.GetFontFamily(index, &family)) ||
        FAILED(family->GetFamilyNames(&names)) ||
        FAILED(names->FindLocaleName(L"en-us", &slot, &found)) ||
        FAILED(names->GetStringLength(found != FALSE ? slot : 0, &length)))
    {
        return {};
    }
    std::wstring wide(length + 1, L'\0');
    if (FAILED(names->GetString(found != FALSE ? slot : 0, wide.data(), length + 1)))
    {
        return {};
    }
    wide.resize(length);
    return core::to_utf8(wide).value_or(std::string{});
}
} // namespace

std::expected<std::vector<std::string>, application::FontFamilyFailure> SystemFontFamilies::list()
{
    Microsoft::WRL::ComPtr<IUnknown> created;
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   created.GetAddressOf())) ||
        FAILED(created.As(&factory)))
    {
        return std::unexpected(application::FontFamilyFailure::unavailable);
    }
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    if (FAILED(factory->GetSystemFontCollection(&collection)))
    {
        return std::unexpected(application::FontFamilyFailure::unlisted);
    }
    std::vector<std::string> names;
    const auto count = collection->GetFontFamilyCount();
    names.reserve(count);
    for (UINT32 index = 0; index < count; ++index)
    {
        auto name = family_name(*collection.Get(), index);
        if (!name.empty())
        {
            names.push_back(std::move(name));
        }
    }
    return names;
}
} // namespace nenenib::ui::win32
