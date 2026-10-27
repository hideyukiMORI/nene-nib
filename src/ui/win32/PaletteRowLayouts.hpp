#pragma once

#include "LayoutRect.hpp"
#include "PaletteLayout.hpp"
#include "PaletteRowKey.hpp"
#include "PaletteRowLayout.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <dwrite.h>
#include <type_traits>
#include <utility>
#include <vector>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// Ctrl+P の面の見えている行の文字組みの保持（#900・ADR 0062 / 0114 の追補）。打鍵の前後で
// 同じ候補の行は layout を作り直さない。上限は palette_row_limit の 4 倍で、越えたら最も前に
// 使ったものから捨てる。書式・DPI の作り直しと面を閉じたときに全部捨てる（device lost は
// renderer ごと作り直すので保持も消える）。生成は呼び出し元の make が受け持ち、作れなかった
// （null の）layout は覚えない。構築では OS 資源を作らない。
class PaletteRowLayouts final
{
  public:
    using TextLayout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;
    static constexpr std::size_t capacity = core::palette_row_limit * 4;

    // 鍵の layout を返す。無ければ make() で作って覚える。幅か高さが 0 以下の欄は make() を
    // 呼ばずに null を返す（描く物が無い欄で layout を作らない）。
    template <typename Make>
        requires std::convertible_to<std::invoke_result_t<Make &>, TextLayout>
    [[nodiscard]] TextLayout obtain(const PaletteRowKey &key, Make &&make)
    {
        if (core::width_of(key.area) <= 0 || core::height_of(key.area) <= 0)
        {
            return {};
        }
        auto found = lookup(key);
        if (found)
        {
            return found;
        }
        TextLayout made = make();
        retain(key, made);
        return made;
    }
    void clear();
    [[nodiscard]] std::size_t size() const noexcept;

  private:
    [[nodiscard]] TextLayout lookup(const PaletteRowKey &key);
    void retain(const PaletteRowKey &key, const TextLayout &layout);

    std::vector<PaletteRowLayout> entries_;
    std::uint64_t generation_ = 0;
};
} // namespace nenenib::ui::win32
