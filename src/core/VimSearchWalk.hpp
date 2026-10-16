#pragma once

#include "TextBuffer.hpp"
#include "VimPattern.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 1 回の検索が本文を歩く道具（#583）。行を std::string に写さず、piece の view を借用して歩く。
// 行が piece をまたぐときだけ、その行を 1 つの再利用する写しに繋ぐ。パターンが魔法の無い
// literal なら、view の上の find / rfind で候補を含む行へ飛ぶ。当たりの判定と位置は照合器の
// まま（ADR 0032）で、ここは見る行を選ぶだけ。行の切れ目は view の上の '\n'、行の内容は
// TextBuffer::line_end と同じく CRLF の本文でだけ末尾の '\r' を外す（ADR 0036）。
// view は本文の借用なので、本文の値より長く生かさない（同期の 1 回の検索の中だけ・ADR 0119）。
// 位置はすべて本文の始まりからのバイトで、行は行の始まりのバイトで指す。
class VimSearchWalk final
{
  public:
    VimSearchWalk(const TextBuffer &text, const VimPattern &pattern);
    VimSearchWalk(const TextBuffer &&text, const VimPattern &pattern) = delete;

    // start の行の内容。次の line_at か候補の問い合わせまで有効。
    [[nodiscard]] std::string_view line_at(std::size_t start);
    // 次の行（最後の行なら値が無い）と前の行（最初の行なら値が無い）。
    [[nodiscard]] std::optional<std::size_t> line_after(std::size_t start) const;
    [[nodiscard]] std::optional<std::size_t> line_before(std::size_t start) const;
    [[nodiscard]] std::size_t last_line() const;
    // 見る価値のある行。literal なら start の行から前向きで最初の候補を含む行、後ろ向きで
    // 最後の候補を含む行（無ければ値が無い）。literal でなければ start の行そのもの。
    [[nodiscard]] std::optional<std::size_t> candidate_after(std::size_t start);
    [[nodiscard]] std::optional<std::size_t> candidate_before(std::size_t start);

  private:
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t span_at(std::size_t at) const noexcept;
    [[nodiscard]] std::size_t newline_after(std::size_t at) const noexcept;
    [[nodiscard]] std::optional<std::size_t> newline_before(std::size_t at) const noexcept;
    [[nodiscard]] std::size_t line_of(std::size_t at) const noexcept;
    [[nodiscard]] std::string_view joined(std::size_t begin, std::size_t end);
    // at 以降に始まる最初の literal と、limit より前に始まる最後の literal。
    [[nodiscard]] std::optional<std::size_t> literal_after(std::string_view literal,
                                                           std::size_t at);
    [[nodiscard]] std::optional<std::size_t> literal_before(std::string_view literal,
                                                            std::size_t limit);
    // edge 番目の view の始まりの端をまたぐ literal。端の前後 literal 長 −1 バイトの窓で拾う。
    [[nodiscard]] std::optional<std::size_t> crossing_after(std::string_view literal,
                                                            std::size_t at, std::size_t edge);
    [[nodiscard]] std::optional<std::size_t> crossing_before(std::string_view literal,
                                                             std::size_t limit, std::size_t edge);

    std::vector<std::string_view> views_;
    // views_ の各 view の本文の中の始まりと、末尾に本文の長さ（views_.size() + 1 個）。
    std::vector<std::size_t> starts_;
    std::optional<std::string> literal_;
    // piece をまたぐ行と端の窓を繋ぐ 1 つの写し。検索の間は確保し直さない。
    std::string scratch_;
    bool crlf_;
};
} // namespace nenenib::core
