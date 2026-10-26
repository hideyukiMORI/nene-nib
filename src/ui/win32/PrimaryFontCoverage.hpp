#pragma once
#include "FallbackRunSource.hpp"
#include "ScriptRunSink.hpp"
#include "TimingPort.hpp"

#include <bitset>
#include <cstddef>
#include <dwrite_3.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 1 行で当てる範囲の数の上限（ADR 0142 の決定 4）。超えた分は DirectWrite の代替に任せる。
inline constexpr std::size_t fallback_range_limit = 4096;

// 本文の 1 つの書式の主字体（書式の family の face）が持つ文字の表と、主字体に無い文字を含む
// 文字種の区切り（DirectWrite の AnalyzeScript）に OS の MapCharacters（書式の FontFallbackCache
// 経由）が選ぶ字体を layout の範囲に先に当てる処理（ADR 0142）。同じ字体の隣は 1
// つの範囲にまとめる。 表は code point
// の出現ごとに遅延して埋める。書式が所有し、書式を作り直すときに捨てる （FontFallbackCache
// と同じ寿命・ADR 0071）。字体を選ぶ規則は持たない。
class PrimaryFontCoverage final
{
  public:
    // 書式の family が collection に無いか、書式に字体選択が付いていなければ nullopt（当てない）。
    // timing は診断の節目の口（experiment/875-diag）。null なら打たない。
    [[nodiscard]] static std::optional<PrimaryFontCoverage>
    of(IDWriteFactory &factory, IDWriteTextFormat &format,
       application::TimingPort *timing = nullptr);
    [[nodiscard]] bool has(char32_t value);
    // layout は format から作ったもので、最初の問い合わせの前に呼ぶ。wide は layout の本文。
    [[nodiscard]] HRESULT assign(IDWriteTextLayout &layout, std::wstring_view wide);

  private:
    PrimaryFontCoverage(IDWriteTextFormat &format, Microsoft::WRL::ComPtr<IDWriteFont> font,
                        Microsoft::WRL::ComPtr<IDWriteFontFallback> fallback,
                        Microsoft::WRL::ComPtr<IDWriteTextAnalyzer> analyzer,
                        application::TimingPort *timing);
    void mark(core::Milestone milestone) const noexcept;
    [[nodiscard]] HRESULT assign_analyzed(IDWriteTextLayout &layout, std::wstring_view wide);
    [[nodiscard]] bool query(char32_t value) const;
    [[nodiscard]] bool missing_at(char32_t value, bool joins, bool previous);
    [[nodiscard]] bool has_missing(std::wstring_view wide, DWRITE_TEXT_RANGE segment);
    [[nodiscard]] HRESULT assign_segment(IDWriteTextLayout &layout, std::wstring_view wide,
                                         DWRITE_TEXT_RANGE segment);
    [[nodiscard]] HRESULT hold(IDWriteTextLayout &layout, IDWriteFont &font, FLOAT scale,
                               DWRITE_TEXT_RANGE range);
    [[nodiscard]] bool joins(IDWriteFont &font, FLOAT scale, DWRITE_TEXT_RANGE range);
    [[nodiscard]] HRESULT flush(IDWriteTextLayout &layout);
    [[nodiscard]] HRESULT apply(IDWriteTextLayout &layout, IDWriteFont &font, FLOAT scale,
                                DWRITE_TEXT_RANGE range);
    [[nodiscard]] const std::wstring &family_of(IDWriteFont &font);

    static constexpr std::size_t basic_plane = 0x10000;
    application::TimingPort *timing_;
    Microsoft::WRL::ComPtr<IDWriteFont> font_;
    Microsoft::WRL::ComPtr<IDWriteFontFace3> face_;
    Microsoft::WRL::ComPtr<IDWriteFontFallback> fallback_;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection_;
    Microsoft::WRL::ComPtr<IDWriteTextAnalyzer> analyzer_;
    Microsoft::WRL::ComPtr<FallbackRunSource> source_;
    Microsoft::WRL::ComPtr<ScriptRunSink> sink_;
    std::wstring family_;
    DWRITE_FONT_WEIGHT weight_;
    DWRITE_FONT_STYLE style_;
    DWRITE_FONT_STRETCH stretch_;
    FLOAT size_;
    // BMP は判定済みと有無の 2 つの bitset、それ以外は小さな map。
    std::unique_ptr<std::bitset<basic_plane>> known_;
    std::unique_ptr<std::bitset<basic_plane>> present_;
    std::unordered_map<char32_t, bool> astral_;
    // 直前に当てた字体と family 名（同じ字体が続くので名前を読み直さない）。
    Microsoft::WRL::ComPtr<IDWriteFont> named_;
    std::wstring named_family_;
    // まだ当てていない範囲（隣の区切りが同じ字体ならここへ伸ばす）と、当てた範囲の数。
    Microsoft::WRL::ComPtr<IDWriteFont> pending_font_;
    std::wstring pending_family_;
    DWRITE_TEXT_RANGE pending_range_{};
    FLOAT pending_scale_ = 1;
    std::size_t applied_ = 0;
};
} // namespace nenenib::ui::win32
