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

// 名前の 1 つ。読めなければ空。
[[nodiscard]] std::string localized_name(IDWriteLocalizedStrings &names, UINT32 slot)
{
    UINT32 length = 0;
    if (FAILED(names.GetStringLength(slot, &length)))
    {
        return {};
    }
    std::wstring wide(length + 1, L'\0');
    if (FAILED(names.GetString(slot, wide.data(), length + 1)))
    {
        return {};
    }
    wide.resize(length);
    return core::to_utf8(wide).value_or(std::string{});
}

// family の全ロケールの名前を足す（既定ロケールは読まない・ARC-007）。読めない名前は飛ばし、
// 重複は core::FontFamilies::from が除く。
void append_family_names(IDWriteFontCollection &collection, UINT32 index,
                         std::vector<std::string> &names)
{
    Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
    Names localized;
    if (FAILED(collection.GetFontFamily(index, &family)) ||
        FAILED(family->GetFamilyNames(&localized)))
    {
        return;
    }
    const auto count = localized->GetCount();
    for (UINT32 slot = 0; slot < count; ++slot)
    {
        auto name = localized_name(*localized.Get(), slot);
        if (!name.empty())
        {
            names.push_back(std::move(name));
        }
    }
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
        append_family_names(*collection.Get(), index, names);
    }
    return names;
}
} // namespace nenenib::ui::win32
