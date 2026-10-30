#include "MemoryXpsTarget.hpp"

namespace nenenib::tests::ui
{
MemoryXpsTarget::MemoryXpsTarget(IXpsOMObjectFactory *factory, IStream *stream)
    : factory_(factory), stream_(stream)
{
}

HRESULT STDMETHODCALLTYPE MemoryXpsTarget::GetXpsOMPackageWriter(
    IOpcPartUri *sequence, IOpcPartUri *discard, IXpsOMPackageWriter **writer) noexcept
{
    return factory_->CreatePackageWriterOnStream(stream_.Get(), TRUE, XPS_INTERLEAVING_OFF,
                                                 sequence, nullptr, nullptr, nullptr, discard,
                                                 writer);
}

HRESULT STDMETHODCALLTYPE MemoryXpsTarget::GetXpsOMFactory(IXpsOMObjectFactory **factory) noexcept
{
    return factory_.CopyTo(factory);
}

HRESULT STDMETHODCALLTYPE MemoryXpsTarget::GetXpsType(XPS_DOCUMENT_TYPE *type) noexcept
{
    *type = XPS_DOCUMENT_TYPE_XPS;
    return S_OK;
}
} // namespace nenenib::tests::ui
