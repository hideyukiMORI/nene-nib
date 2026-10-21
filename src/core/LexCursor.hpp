#pragma once

#include "ColorSpan.hpp"
#include "LexLine.hpp"
#include "LexState.hpp"
#include "SyntaxRole.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 字句器が 1 行を読む間の書き手（#815）。区間はバイトの位置で受けて、その場で code point の桁
// （行頭が Column{1}）へ写す。区間は昇順に来るので、数えた所から先だけを数える。
class LexCursor
{
  public:
    explicit LexCursor(std::string_view line);

    [[nodiscard]] std::string_view line() const noexcept;
    // [begin, end) のバイトの区間を role で塗る。空の区間は捨てる。begin は前の区間の end 以上。
    void mark(std::size_t begin, std::size_t end, SyntaxRole role);
    // 行の終わりで次の行へ渡す状態。呼ばなければ plain。
    void leave(const LexState &state) noexcept;
    [[nodiscard]] LexLine finish() &&;

  private:
    [[nodiscard]] Column column_at(std::size_t at) noexcept;

    std::string_view line_;
    std::vector<ColorSpan> spans_;
    std::size_t counted_ = 0U;
    Column column_{1U};
    LexState out_{};
};
} // namespace nenenib::core
