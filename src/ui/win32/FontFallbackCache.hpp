#pragma once
#include "FontFallbackEntry.hpp"
#include "TimingPort.hpp"
#include <cstddef>
#include <vector>
#include <wrl/implements.h>
namespace nenenib::ui::win32
{
// システムの選択結果だけを保存する。文字組みと字形の区切りはDirectWriteのまま。
class FontFallbackCache final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteFontFallback>
{
  public:
    // timing は診断の節目の口（experiment/875-diag）。null なら打たない。
    explicit FontFallbackCache(Microsoft::WRL::ComPtr<IDWriteFontFallback> system,
                               application::TimingPort *timing = nullptr);
    HRESULT STDMETHODCALLTYPE MapCharacters(IDWriteTextAnalysisSource *source, UINT32 position,
                                            UINT32 length, IDWriteFontCollection *collection,
                                            const WCHAR *family, DWRITE_FONT_WEIGHT weight,
                                            DWRITE_FONT_STYLE style, DWRITE_FONT_STRETCH stretch,
                                            UINT32 *mapped_length, IDWriteFont **mapped_font,
                                            FLOAT *scale) noexcept override;

  private:
    [[nodiscard]] HRESULT map(const FontFallbackRequest &request, UINT32 *mapped_length,
                              IDWriteFont **mapped_font, FLOAT *scale);
    [[nodiscard]] const FontFallbackEntry *find(const FontFallbackKey &key) const;
    void retain(FontFallbackEntry entry);
    void mark(core::Milestone milestone) const noexcept;
    application::TimingPort *timing_;
    Microsoft::WRL::ComPtr<IDWriteFontFallback> system_;
    std::vector<FontFallbackEntry> entries_;
    std::size_t next_ = 0;
};
} // namespace nenenib::ui::win32
