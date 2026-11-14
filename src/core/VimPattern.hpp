#pragma once

#include "VimPatternAtom.hpp"
#include "VimPatternCaptures.hpp"
#include "VimPatternCase.hpp"
#include "VimPatternFailure.hpp"
#include "VimPatternGroup.hpp"
#include "VimPatternInstruction.hpp"
#include "VimPatternLimits.hpp"
#include "VimPatternMatch.hpp"
#include "VimPatternRange.hpp"
#include "VimPatternRoute.hpp"
#include "VimSearchDirection.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
struct VimPatternParse;

// Vim の `magic` の部分集合の照合器（ADR 0032 の決定 4）。`std::regex` と `<locale>` は
// 使わない（ロケール依存のシンボルが core の外へ出る）。一致は 1 行の中だけで、行をまたがない。
// 生成経路は parse ただ 1 つで、未対応の構文は閉じた失敗で拒否する（CPP-005 / CPP-007）。
class VimPattern final
{
  public:
    // separator は打たれた向き。前向きの `/` と後ろ向きの `?` の区切りがパターンの中に
    // 現れたら offset として拒否する（offset は未対応）。正しい UTF-8 でない pattern は
    // invalid_utf8 で拒否する（#906）。letter_case は大小の旗（#1041）で、パターンの中の `\c`
    // （区別しない）と `\C`（区別する）が旗より強く、両方あれば `\c` が勝つ（Vim 9.1 の実測）。
    // smart の「大文字がある」は `\` に続く文字を数えない（`\S` `\C` は数えない・Vim の
    // pat_has_uppercase）。群 `\(` `\)` は 9 個まで・入れ子可で、閉じない `\(`・対の無い `\)`・
    // 10 個目・群の後ろの `*`（群の繰り返し）は group / quantifier で拒否する（ADR 0168 の決定
    // 4）。
    // `\(` の直後の `*` は文字、`^` は錨、`\)` の直前の `$` は錨（Vim の magic と同じ）。
    // 原子 1 つに付く量 `\+` `\=` `\?` `\{n}` `\{n,}` `\{,m}` `\{n,m}` と最短の `\{-...}` は
    // 原子の写しに開く（ADR 0184 の決定 2）。`\{` の中の不正は brace（E554）、付く原子の無い量は
    // misplaced（E866・`\c` `\C` の直後も）、量の後ろの量（`a**` も）は multi_after_multi
    // （E871）、開いた後の原子の数が limits.atoms（製品は vim_pattern_atom_limit・
    // 試験だけが小さな値を渡す）を超えれば too_complex。幅の無い原子に付く量は、
    // 最小が 1 以上なら原子 1 つ、0 なら原子を消す（`^\+a` は `^a`・Vim 9.1 の実測）。
    // 選択 `\|`・番号の無い群 `\%(` `\)`・群に付く量（ADR 0184 の決定 3）を受け、
    // 選択か群の量を含むパターンだけを命令の列に組む（決定 1）。
    // 命令の数が limits.instructions（製品は vim_pattern_instruction_limit）を
    // 超えれば too_complex。閉じない `\(` は group（E54）、
    // 閉じない `\%(` は unclosed_plain_group（E53）、
    // 対の無い `\)` は unmatched_close（E55）、10 個目の `\(` は too_many_groups（E872）。
    // `\zs` `\ze`（ADR 0184 の決定 5）を含むパターンも命令の列に組み、直後の `*` `\+` `\{` は
    // repeated_match_begin / repeated_match_end（E888）で断る。
    [[nodiscard]] static std::expected<VimPattern, VimPatternFailure>
    parse(std::string_view pattern, VimSearchDirection separator, VimPatternCase letter_case,
          VimPatternLimits limits = {});

    // line の from バイト以降で最も左の一致（ADR 0032 の決定 4 の追記）。長さ 0 の一致も返す。
    // line（matched_sequence も）は検証済みの UTF-8 が前提で、行ごとには確かめない（#906）。
    [[nodiscard]] std::optional<VimPatternMatch> matched(std::string_view line,
                                                         std::size_t from) const;

    // 一致の長さの上限（バイト・#556）。食べる原子は 1 code point（UTF-8 で 4 バイトまで・cluster
    // は基底と結合文字の数だけ）を食べ、幅の無い原子は何も食べない。上限の無い反復（`*`
    // `\{-}`）を含めば決まらないので値が無い。`\=` の写しは 1 回ぶんを数える。
    [[nodiscard]] std::optional<std::size_t> longest_bytes() const noexcept;

    // 魔法の無い文字だけの列なら、その UTF-8 のバイト列（#583）。このパターンが行に当たるのは
    // この列が行に現れるときだけなので、検索は候補の行の絞り込みに使える（当たりの位置は照合器が
    // 決める）。空のパターンと、literal 以外の原子や `*` を含むパターンは値が無い。大小を畳む
    // パターンは、英字か ASCII の外の文字を含めば値が無い（バイト列の一致で絞れない・#1041）。
    [[nodiscard]] std::optional<std::string> literal_bytes() const;

    // 照合器が通る道（ADR 0184 の決定 1）。matched と VimPatternEvaluation はこの答えで道を選ぶ。
    // 試験が「量の無いパターンは今の道のまま」を直接見るための口でもある。
    [[nodiscard]] VimPatternRoute route() const noexcept;
    // `\zs` `\ze` を含むか（ADR 0184 の決定 5）。含むパターンは命令の列の道を通り、当たりの
    // begin と end（置き直した範囲）が whole_begin と whole_end（パターン全体の範囲）と違いうる
    // （「at 以降に始まる当たり」を引く呼び手は、照合の始まりではなく置き直した始まりで比べる）。
    [[nodiscard]] bool bounded() const noexcept;
    // 量を開いた後の原子の数（ADR 0184 の決定 1・2・試験が parse の結果を見る口）。
    [[nodiscard]] std::size_t atom_count() const noexcept;
    // 命令の列の命令の数（命令の列の道でなければ 0・ADR 0184 の決定 3・試験が parse
    // の結果を見る口）。
    [[nodiscard]] std::size_t instruction_count() const noexcept;
    // 群 `\(` `\)` の数（ADR 0168 の決定 4・0〜9）。
    [[nodiscard]] std::size_t group_count() const noexcept;

    // match（この line への matched の答え）の群の範囲（ADR 0168 の決定 4・追補 13）。0 は match
    // そのもの。照合器と同じ優先順（`*` は長い方から・前の原子が先）で match の始まりから終わり
    // までの道を 1 本だけ探し（VimPatternCapture・仕事は升の数に比例）、その道で群の端を通った
    // 位置を返す。群の無いパターンは歩かない。升の数（原子の数 + 1）×（一致の長さ + 1）が
    // cell_limit（製品は vim_capture_cell_limit・試験だけが小さな値を渡す）を超える一致は探さずに
    // 値なしを返す（群を黙って空にしない・呼ぶ側が置換全体を断る）。
    [[nodiscard]] std::optional<VimPatternCaptures>
    captured(std::string_view line, VimPatternMatch match,
             std::size_t cell_limit = vim_capture_cell_limit) const;

  private:
    friend class VimPatternEvaluation;
    friend class VimPatternCapture;
    friend class VimPatternMachine;
    friend class VimPatternTrace;

    VimPattern(VimPatternParse &&parse, std::vector<VimPatternInstruction> program,
               bool ignore_case);
    // 命令の列の道の群の範囲（VimPatternTrace・ADR 0184 の決定 4）。
    [[nodiscard]] std::optional<VimPatternCaptures>
    traced(std::string_view line, VimPatternMatch match, std::size_t cell_limit) const;
    // 量の無い列の道（最も左の一致・ADR 0184 の決定 1）。
    [[nodiscard]] std::optional<VimPatternMatch> sequenced(std::string_view line,
                                                           std::size_t from) const;
    // 反復のない原子列を順に評価する。選択肢がないので状態列の確保も再帰も不要。
    [[nodiscard]] std::optional<std::size_t> matched_sequence(std::string_view line,
                                                              std::size_t at) const;
    [[nodiscard]] static bool is_zero_width(VimPatternAtomKind kind) noexcept;
    // 量の 3 つの問い（ADR 0184 の決定 2）: 食べた後に同じ原子へ戻るか（`*` `\{-}`）・0 回で
    // 先へ進めるか・0 回の枝を先に試すか（最短）。照合器と群の道の探索が同じ答えを読む。
    [[nodiscard]] static bool loops(VimPatternRepeat repeat) noexcept;
    [[nodiscard]] static bool skippable(VimPatternRepeat repeat) noexcept;
    [[nodiscard]] static bool lazy(VimPatternRepeat repeat) noexcept;
    [[nodiscard]] static bool anchored(std::string_view line, const VimPatternAtom &atom,
                                       std::size_t at);
    // 位置が Vim の 1 文字の境か（本文の側にだけ結合文字が続けば偽・#1138）。一致の終わりと
    // 始まり（#1146）と `\<` `\>` はここでだけ真になる。
    [[nodiscard]] static bool ends_a_character(std::string_view line, std::size_t end) noexcept;
    // at の code point を食べた後の位置（`.` と集合は 1 文字の残りの結合文字まで・#1146）。
    [[nodiscard]] static std::size_t eaten(std::string_view line, VimPatternAtomKind kind,
                                           std::size_t at);
    [[nodiscard]] static bool whole_character(VimPatternAtomKind kind) noexcept;
    // 原子が line の at の code point（code は呼び手が 1 回だけ読んだその値）を食べられるか
    // （食べた後の位置は eaten）。line と at は結合文字だけの原子が同じ 1
    // 文字の続きを読む（#1506）。
    [[nodiscard]] bool consumes(std::string_view line, const VimPatternAtom &atom, std::size_t at,
                                char32_t code) const;
    // cluster（ADR 0184 の決定 2）の結合文字が at の基底の後ろにちょうど並ぶか。
    [[nodiscard]] bool spells_cluster(std::string_view line, const VimPatternAtom &atom,
                                      std::size_t at) const;
    // 結合文字だけの原子（#1506）が at から始まる 1 文字に当たるか。
    [[nodiscard]] bool carries_marks(std::string_view line, const VimPatternAtom &atom,
                                     std::size_t at) const;
    // 1 文字を食べる原子。幅の無い原子は何も食べない。
    [[nodiscard]] std::optional<std::size_t>
    stepped(std::string_view line, const VimPatternAtom &atom, std::size_t at) const;
    [[nodiscard]] bool in_set(const VimPatternAtom &atom, char32_t code) const;
    [[nodiscard]] bool in_ranges(const VimPatternAtom &atom, char32_t code) const;

    std::vector<VimPatternAtom> atoms_;
    std::vector<VimPatternRange> ranges_;
    // 番号のある群（`\(` の順）。照合は読まず、captured
    // だけが読む（命令の列の道では数だけを読む）。
    std::vector<VimPatternGroup> groups_;
    // 大小を畳んで比べるか（parse が旗と `\c` `\C` から 1 回だけ決める）。
    bool ignore_case_;
    // 最短の量（`\{-...}`）の写しを含むか（ADR 0184 の決定 2・照合器が展開の道を選ぶ）。
    bool lazy_;
    // 命令の列（選択か群の量を含むパターンだけ・ほかは空・ADR 0184 の決定 3）。
    std::vector<VimPatternInstruction> program_;
    // 通る道（構築で 1 回だけ決める・ADR 0184 の決定 1）。
    VimPatternRoute route_;
    // 命令の列に `\zs` `\ze` があるか（構築で 1 回だけ決める・ADR 0184 の決定 5）。
    bool bounded_;
    // 命令の列の一致の長さの上限（longest_bytes の答え・命令の列の道だけが読む）。
    std::optional<std::size_t> program_longest_;
};
} // namespace nenenib::core
