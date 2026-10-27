#pragma once
#include <cstddef>
#include <cstdint>
#include <dwrite.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 覚える区切りの片の数の上限（ADR 0142 の追補）。超えたら全部捨てて覚え直す。
inline constexpr std::size_t fallback_segment_memo_limit = 4096;

// 主字体に無い文字を含む区切りの片の文字列ごとに、OS の字体選択（MapCharacters）の S_OK の答え
// （字体・scale・mappedLength）を覚える（ADR 0142 の追補）。鍵は片の文字列だけで、書式の family・
// weight・style・stretch・言語・向きは持ち主の PrimaryFontCoverage と同じ寿命で一定。字体は null
// （どの字体にも無い）も覚える。探索は FNV-1a（64 ビット）の開番地法で、引くときは確保しない。
// 字体は正規の字体の表の行の番号で持つ（Issue #889）。行は (english family 名・weight・style・
// stretch) の組ごとに 1 つで、OS が同じ組で別の IDWriteFont を返しても同じ行になるので、行の番号が
// 等しいことは組が等しいことと同じ。表は覚えを捨てても残す（組の数は字体の数を超えない）。
class FallbackSegmentMemo final
{
  public:
    FallbackSegmentMemo();
    [[nodiscard]] std::optional<std::size_t> find(std::wstring_view text) const;
    // 覚えの entry の正規の字体の行。null の字体（どの字体にも無い）は nullopt。
    [[nodiscard]] std::optional<std::size_t> font(std::size_t entry) const;
    [[nodiscard]] FLOAT scale(std::size_t entry) const;
    [[nodiscard]] UINT32 mapped(std::size_t entry) const;
    [[nodiscard]] std::size_t size() const noexcept;
    // 空の文字列・鍵の上限（fallback_key_text_limit）を超える文字列・覚え済みの文字列は覚えない。
    void remember(std::wstring_view text, std::optional<std::size_t> font, FLOAT scale,
                  UINT32 mapped);
    // 字体の正規の行。表にある物はその行、新しい物は family 名を 1 回だけ読んで同じ組の行を探し、
    // 無ければ足す。null の字体と family 名の読めない字体は nullopt（当てない・DirectWrite
    // の代替）。
    [[nodiscard]] std::optional<std::size_t>
    normalized(const Microsoft::WRL::ComPtr<IDWriteFont> &font);
    [[nodiscard]] const Microsoft::WRL::ComPtr<IDWriteFont> &row_font(std::size_t row) const;
    [[nodiscard]] const std::wstring &row_family(std::size_t row) const;
    [[nodiscard]] DWRITE_FONT_WEIGHT row_weight(std::size_t row) const;
    [[nodiscard]] DWRITE_FONT_STYLE row_style(std::size_t row) const;
    [[nodiscard]] DWRITE_FONT_STRETCH row_stretch(std::size_t row) const;
    [[nodiscard]] std::size_t rows() const noexcept;

  private:
    [[nodiscard]] std::size_t slot_of(std::wstring_view text) const;
    [[nodiscard]] std::optional<std::size_t> row_of(const std::wstring &family,
                                                    DWRITE_FONT_WEIGHT weight,
                                                    DWRITE_FONT_STYLE style,
                                                    DWRITE_FONT_STRETCH stretch) const;
    void forget();

    // 番地の数は上限の 2 倍（2 の冪）。番地の値は 0 が空、ほかは entry の番号 + 1。
    std::vector<std::uint32_t> slots_;
    std::vector<std::wstring> texts_;
    std::vector<std::optional<std::size_t>> fonts_;
    std::vector<FLOAT> scales_;
    std::vector<UINT32> mapped_;
    // 正規の字体の表（行ごとに平行な列）。
    std::vector<Microsoft::WRL::ComPtr<IDWriteFont>> row_fonts_;
    std::vector<std::wstring> row_families_;
    std::vector<DWRITE_FONT_WEIGHT> row_weights_;
    std::vector<DWRITE_FONT_STYLE> row_styles_;
    std::vector<DWRITE_FONT_STRETCH> row_stretches_;
};
} // namespace nenenib::ui::win32
