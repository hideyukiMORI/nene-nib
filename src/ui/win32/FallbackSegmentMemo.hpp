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
class FallbackSegmentMemo final
{
  public:
    FallbackSegmentMemo();
    [[nodiscard]] std::optional<std::size_t> find(std::wstring_view text) const;
    [[nodiscard]] const Microsoft::WRL::ComPtr<IDWriteFont> &font(std::size_t entry) const;
    [[nodiscard]] FLOAT scale(std::size_t entry) const;
    [[nodiscard]] UINT32 mapped(std::size_t entry) const;
    [[nodiscard]] std::size_t size() const noexcept;
    // 空の文字列・鍵の上限（fallback_key_text_limit）を超える文字列・覚え済みの文字列は覚えない。
    void remember(std::wstring_view text, Microsoft::WRL::ComPtr<IDWriteFont> font, FLOAT scale,
                  UINT32 mapped);

  private:
    [[nodiscard]] std::size_t slot_of(std::wstring_view text) const;
    void forget();

    // 番地の数は上限の 2 倍（2 の冪）。番地の値は 0 が空、ほかは entry の番号 + 1。
    std::vector<std::uint32_t> slots_;
    std::vector<std::wstring> texts_;
    std::vector<Microsoft::WRL::ComPtr<IDWriteFont>> fonts_;
    std::vector<FLOAT> scales_;
    std::vector<UINT32> mapped_;
};
} // namespace nenenib::ui::win32
