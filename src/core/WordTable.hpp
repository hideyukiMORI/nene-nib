#pragma once

#include "Edit.hpp"
#include "TextBuffer.hpp"
#include "WordCount.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 語の表を作る本文の上限（ADR 0169 の決定 1 と設計席の追補 2026-11-18）。これを超える本文には表を
// 作らない（今の文書の候補はキャレットの前後の窓の語だけ・限界）。
inline constexpr std::size_t word_table_byte_limit = std::size_t{1024} * 1024U;
// 1 回の編集で消える＋入るバイトの上限。超えたら増分で直さず表を捨てる（`:%s` や全消しで止まらない
// ように・次に補完を使ったときに作り直す）。
inline constexpr std::size_t word_table_edit_limit = std::size_t{64} * 1024U;

// 文書ごとの語の表（ADR 0169 の決定 1）。語（vim_keyword_runs の 2 文字以上の語）→ 出現の数。
// 最初に補完を使ったときに of で 1 回作り（開く道では作らない）、その後の編集は edited が編集の
// 触れた語だけを読み直す（1 編集で読むのは編集の前後の語の文字の連なりと消えた・入った本文だけ・
// 全文を走らない）。
//
// 状態（EditorState）は打鍵ごとに写るので、値の写しは共有の 2 本の参照だけにする。base_ は作った
// ときの表（語の順・不変）、recent_ はその後の増減（語の順・数は負もある・0 の行は持たない）で、
// recent_ が recent_word_limit を超えたら base_ へ畳む。どちらも語のバイト列の順に並べ、引くのは
// 二分探索（std::lower_bound）。
class WordTable
{
  public:
    // 増減の列を base_ へ畳む行の数。1 編集で写す量（recent_ の写し）と畳む頻度の釣り合い。
    static constexpr std::size_t recent_word_limit = 128;

    // text の語の表。text が word_table_byte_limit を超えれば値なし。
    [[nodiscard]] static std::optional<WordTable> of(const TextBuffer &text);

    // before に edit（before の上の at から removed を消して inserted
    // を入れた編集）を当てた後の表。
    [[nodiscard]] WordTable edited(const TextBuffer &before, const Edit &edit) const;

    // 語 word の出現の数（無ければ 0）。
    [[nodiscard]] std::int64_t count(std::string_view word) const;

    // prefix で始まる語を語のバイト列の順に limit 個まで（数は正のものだけ）。
    [[nodiscard]] std::vector<WordCount> prefixed(std::string_view prefix, std::size_t limit) const;

    // 畳む前の増減の行の数（試験が畳みを確かめる）。
    [[nodiscard]] std::size_t recent_size() const noexcept;

  private:
    using Counts = std::vector<WordCount>;

    WordTable(std::shared_ptr<const Counts> base, std::shared_ptr<const Counts> recent);

    std::shared_ptr<const Counts> base_;
    std::shared_ptr<const Counts> recent_;
};
} // namespace nenenib::core
