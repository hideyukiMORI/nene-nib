// ID_DOCUMENTPACKAGETARGET_MSXPS は DocumentTarget.h の DEFINE_GUID
// なので、この翻訳単位で定義を置く。
#include <initguid.h>

#include <DocumentTarget.h>

#include "MemoryPackageTarget.hpp"

#include <objbase.h>

namespace nenenib::tests::ui
{
MemoryPackageTarget::MemoryPackageTarget(IXpsDocumentPackageTarget *xps) : xps_(xps) {}

HRESULT STDMETHODCALLTYPE MemoryPackageTarget::GetPackageTargetTypes(UINT32 *count,
                                                                     GUID **types) noexcept
{
    *types = static_cast<GUID *>(CoTaskMemAlloc(sizeof(GUID)));
    if (*types == nullptr)
    {
        *count = 0;
        return E_OUTOFMEMORY;
    }
    **types = ID_DOCUMENTPACKAGETARGET_MSXPS;
    *count = 1;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE MemoryPackageTarget::GetPackageTarget(REFGUID type, REFIID wanted,
                                                                void **target) noexcept
{
    if (type != ID_DOCUMENTPACKAGETARGET_MSXPS)
    {
        *target = nullptr;
        return E_NOINTERFACE;
    }
    return xps_->QueryInterface(wanted, target);
}

HRESULT STDMETHODCALLTYPE MemoryPackageTarget::Cancel() noexcept
{
    ++cancelled_;
    return S_OK;
}

unsigned MemoryPackageTarget::cancelled() const noexcept
{
    return cancelled_;
}
} // namespace nenenib::tests::ui
