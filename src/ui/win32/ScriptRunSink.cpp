#include "ScriptRunSink.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::ui::win32
{
std::vector<DWRITE_TEXT_RANGE> ScriptRunSink::take()
{
    std::ranges::sort(runs_, [](const DWRITE_TEXT_RANGE &left, const DWRITE_TEXT_RANGE &right)
                      { return left.startPosition < right.startPosition; });
    return std::exchange(runs_, {});
}

HRESULT STDMETHODCALLTYPE ScriptRunSink::SetScriptAnalysis(UINT32 position, UINT32 length,
                                                           const DWRITE_SCRIPT_ANALYSIS *) noexcept
{
    runs_.push_back(DWRITE_TEXT_RANGE{position, length});
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ScriptRunSink::SetLineBreakpoints(UINT32, UINT32,
                                                            const DWRITE_LINE_BREAKPOINT *) noexcept
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ScriptRunSink::SetBidiLevel(UINT32, UINT32, UINT8, UINT8) noexcept
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ScriptRunSink::SetNumberSubstitution(UINT32, UINT32,
                                                               IDWriteNumberSubstitution *) noexcept
{
    return S_OK;
}
} // namespace nenenib::ui::win32
