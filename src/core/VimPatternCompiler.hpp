#pragma once

#include "VimPatternAtom.hpp"
#include "VimPatternFailure.hpp"
#include "VimPatternInstruction.hpp"
#include "VimPatternMark.hpp"

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

namespace nenenib::core
{
// 群の入れ子の深さの上限。組み立ては群ごとに再帰するので、深さを止めて呼び出しの積みを守る
// （`\%(` の入れ子は番号の上限 9 に掛からない）。超えるパターンは too_complex で断る。
inline constexpr std::size_t vim_pattern_nesting_limit = 64;

// 原子の列と構造の印（VimPatternMark）から命令の列を組む（ADR 0184 の決定 3）。選択は Vim 9.1 の
// NFA_OR と同じく先の枝が先の split の連なり、群に付く量は Vim の nfa_regpiece と同じ写し（`\+` は
// 1 回と `*`・`\{n,m}` は n 回と (m - n) 個の `\=`・`\{n,}` は n 回と `*`・`\{0}`
// は群を消す・最短は split の先と後を入れ替える）。原子に付く量（parse が開いた写し）も split と
// jump に開く。生成と 操作は VimPattern::parse に閉じる。
class VimPatternCompiler final
{
  public:
    [[nodiscard]] static std::expected<std::vector<VimPatternInstruction>, VimPatternFailure>
    compiled(std::span<const VimPatternAtom> atoms, std::span<const VimPatternMark> marks,
             std::size_t instruction_limit);

  private:
    VimPatternCompiler(std::span<const VimPatternAtom> atoms, std::span<const VimPatternMark> marks,
                       std::size_t instruction_limit);
    // 原子と印を 1 本の並び（tokens_）にし、開きの印から対の閉じの位置を引く表（partners_）を作る。
    void tokens_built();
    [[nodiscard]] bool is_mark(std::size_t token) const noexcept;
    [[nodiscard]] const VimPatternMark &mark_of(std::size_t token) const;
    // 並びの [begin, end) を選択として組む（枝が 1 つなら列のまま）。
    void alternation(std::size_t begin, std::size_t end);
    // 選択の無い並びの [begin, end) を組む。
    void sequence(std::size_t begin, std::size_t end);
    void atom_emitted(const VimPatternAtom &atom, std::size_t index);
    // open と close は並びの中の群の開きと閉じの位置。
    void group_emitted(std::size_t open, std::size_t close);
    void body_emitted(std::size_t open, std::size_t close);
    // 量の残りの写し 1 つ（loops なら `*` の輪・ほかは `\=`）。
    void optional_body_emitted(std::size_t open, std::size_t close, bool loops, bool lazy);
    std::size_t emitted(VimPatternInstruction instruction);
    void split_aimed(std::size_t split, std::size_t body, std::size_t after, bool lazy);

    std::span<const VimPatternAtom> atoms_;
    std::span<const VimPatternMark> marks_;
    std::size_t limit_;
    std::size_t depth_ = 0;
    bool overflowed_ = false;
    // 原子の番号（原子の数より小さい）か、原子の数 + 印の番号。
    std::vector<std::size_t> tokens_;
    // 並びの位置ごとの対の閉じの位置（開きの印の位置だけが読む）。
    std::vector<std::size_t> partners_;
    std::vector<VimPatternInstruction> program_;
};
} // namespace nenenib::core
