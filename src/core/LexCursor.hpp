#pragma once

#include "ColorSpan.hpp"
#include "LexLine.hpp"
#include "LexOutput.hpp"
#include "LexPrecedent.hpp"
#include "LexState.hpp"
#include "SyntaxRole.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 字句器が 1 行を読む間の書き手（#815）。区間はバイトの位置で受けて、その場で code point の桁
// （行頭が Column{1}）へ写す。区間は昇順に来るので、数えた所から先だけを数える。桁の数え方は
// lenient_code_point_count の 1 本で、不正な UTF-8 のバイトも 1 つで 1 桁（#878）。state_only では
// 区間を捨て（桁も数えない）、状態だけを残す。
class LexCursor
{
  public:
    LexCursor(std::string_view line, LexOutput output);

    [[nodiscard]] std::string_view line() const noexcept;
    // [begin, end) のバイトの区間を role で塗る。空の区間は捨てる。begin は前の区間の end 以上。
    void mark(std::size_t begin, std::size_t end, SyntaxRole role);
    // 行の終わりで次の行へ渡す状態。呼ばなければ plain。
    void leave(const LexState &state) noexcept;
    [[nodiscard]] const LexState &state() const noexcept;
    // 直前の字句の種類（#833・JavaScript の `/` の見分け）。行頭は expression_start。区間を捨てる
    // state_only でも同じに覚え、lex_line と lex_state が同じ状態機械を通る。
    void follow(LexPrecedent precedent) noexcept;
    [[nodiscard]] LexPrecedent precedent() const noexcept;
    [[nodiscard]] LexLine finish() &&;

  private:
    [[nodiscard]] Column column_at(std::size_t at) noexcept;

    std::string_view line_;
    LexOutput output_;
    std::vector<ColorSpan> spans_;
    std::size_t counted_ = 0U;
    Column column_{1U};
    LexState out_{};
    LexPrecedent precedent_ = LexPrecedent::expression_start;
};
} // namespace nenenib::core
