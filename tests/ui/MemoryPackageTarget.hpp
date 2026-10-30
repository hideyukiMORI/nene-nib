#pragma once

#include <DocumentTarget.h>
#include <XpsObjectModel_1.h>
#include <wrl/client.h>
#include <wrl/implements.h>

namespace nenenib::tests::ui
{
// IPrintDocumentPackageTargetFactory の job の受け口の替え玉（#1047・ADR 0177 の決定 8）。種類は
// MSXPS の 1 つだけを答え、XPS の受け口は MemoryXpsTarget に渡す。Cancel を数える。
class MemoryPackageTarget final : public Microsoft::WRL::RuntimeClass<
                                      Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                      IPrintDocumentPackageTarget>
{
  public:
    explicit MemoryPackageTarget(IXpsDocumentPackageTarget *xps);
    HRESULT STDMETHODCALLTYPE GetPackageTargetTypes(UINT32 *count, GUID **types) noexcept override;
    HRESULT STDMETHODCALLTYPE GetPackageTarget(REFGUID type, REFIID wanted,
                                               void **target) noexcept override;
    HRESULT STDMETHODCALLTYPE Cancel() noexcept override;
    [[nodiscard]] unsigned cancelled() const noexcept;

  private:
    Microsoft::WRL::ComPtr<IXpsDocumentPackageTarget> xps_;
    unsigned cancelled_ = 0;
};
} // namespace nenenib::tests::ui
