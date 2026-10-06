#include "DirectWriteFactory.hpp"

namespace nenenib::tests::ui
{
Microsoft::WRL::ComPtr<IDWriteFactory2> directwrite_factory()
{
    Microsoft::WRL::ComPtr<IUnknown> unknown;
    Microsoft::WRL::ComPtr<IDWriteFactory2> factory;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2),
                                   unknown.GetAddressOf())) ||
        FAILED(unknown.As(&factory)))
    {
        return nullptr;
    }
    return factory;
}

bool installed(IDWriteFactory2 &factory, const wchar_t *family)
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    UINT32 index = 0;
    BOOL present = FALSE;
    return SUCCEEDED(factory.GetSystemFontCollection(collection.GetAddressOf(), FALSE)) &&
           SUCCEEDED(collection->FindFamilyName(family, &index, &present)) && present != FALSE;
}
} // namespace nenenib::tests::ui
