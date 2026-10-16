#pragma once
#include <dwrite.h>
#include <vector>
#include <wrl/implements.h>
namespace nenenib::ui::win32
{
// DirectWrite の文字種の解析（AnalyzeScript）の区切りを受ける（ADR 0142 の決定 2）。
// FallbackRunSource と対で、区切りの位置と長さだけを持つ。字体は選ばない。
class ScriptRunSink final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextAnalysisSink>
{
  public:
    // 受けた区切りを本文の順に渡し、受け皿を空にする。
    [[nodiscard]] std::vector<DWRITE_TEXT_RANGE> take();

    HRESULT STDMETHODCALLTYPE SetScriptAnalysis(
        UINT32 position, UINT32 length, const DWRITE_SCRIPT_ANALYSIS *analysis) noexcept override;
    HRESULT STDMETHODCALLTYPE
    SetLineBreakpoints(UINT32 position, UINT32 length,
                       const DWRITE_LINE_BREAKPOINT *breakpoints) noexcept override;
    HRESULT STDMETHODCALLTYPE SetBidiLevel(UINT32 position, UINT32 length, UINT8 explicit_level,
                                           UINT8 resolved_level) noexcept override;
    HRESULT STDMETHODCALLTYPE SetNumberSubstitution(
        UINT32 position, UINT32 length, IDWriteNumberSubstitution *substitution) noexcept override;

  private:
    std::vector<DWRITE_TEXT_RANGE> runs_;
};
} // namespace nenenib::ui::win32
