#pragma once

#include <XpsObjectModel_1.h>
#include <objidl.h>
#include <wrl/client.h>
#include <wrl/implements.h>

namespace nenenib::tests::ui
{
// print control が XPS の package を書く先（#1047・ADR 0177 の決定 8）。OS のスプールの代わりに、
// XPS の object model の package writer をメモリの stream に開いて渡す。実機のプリンタもファイル
// も使わない（ADR 0077）。
class MemoryXpsTarget final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IXpsDocumentPackageTarget>
{
  public:
    MemoryXpsTarget(IXpsOMObjectFactory *factory, IStream *stream);
    HRESULT STDMETHODCALLTYPE GetXpsOMPackageWriter(IOpcPartUri *sequence, IOpcPartUri *discard,
                                                    IXpsOMPackageWriter **writer) noexcept override;
    HRESULT STDMETHODCALLTYPE GetXpsOMFactory(IXpsOMObjectFactory **factory) noexcept override;
    HRESULT STDMETHODCALLTYPE GetXpsType(XPS_DOCUMENT_TYPE *type) noexcept override;

  private:
    Microsoft::WRL::ComPtr<IXpsOMObjectFactory> factory_;
    Microsoft::WRL::ComPtr<IStream> stream_;
};
} // namespace nenenib::tests::ui
