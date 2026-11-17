#pragma once

#include "Column.hpp"
#include "LineNumber.hpp"

#include <cstddef>
#include <string>

namespace nenenib::core
{
// プロジェクトの全文検索の当たり 1 つ（ADR 0167 の決定 3・ADR 0189 の当たりの行）。1 行に当たりは
// 1 つ（その行の最も左の当たり・Vim の `:vimgrep` の g の無い形）。行の本文は改行と行末の CR を
// 除き、ファイルの先頭の UTF-8 の BOM も除いた形（文書を開いた本文と同じ行と桁になる）。
// line と column は開いた後に PlaceCaret の TextPosition へそのまま渡す単位。
// line_text は面の行に描く窓で、match_begin と match_end はその中の当たり（色を置く範囲）。
struct ProjectHit
{
    // 当たりの行（1 始まり・`\n` で区切った行の番号）。
    LineNumber line;
    // 当たりが始まる桁（code point・1 始まり・行頭から数える。窓ではなく行の全体での桁）。
    Column column;
    // 行の本文の窓（最大 project_hit_text_bytes バイト・正しい UTF-8・code point を割らない）。
    // 当たりが行頭からの窓に収まらなければ、当たりの前に project_hit_context_bytes を残して
    // 当たりの側へ寄せる。
    std::string line_text;
    // line_text が行頭から何バイト目で始まるか（バイト・0 なら行頭から。正なら行頭を省いた窓で、
    // application が省いた印を足す）。
    std::size_t text_begin;
    // line_text の中の当たりの半開区間（バイト・line_text の先頭から）。当たりが窓の終わりを
    // 越えれば match_end は line_text.size()。長さ 0 の当たり（`/^` など）では等しい。
    std::size_t match_begin;
    std::size_t match_end;
};

[[nodiscard]] inline bool operator==(const ProjectHit &left, const ProjectHit &right) noexcept
{
    return left.line == right.line && left.column == right.column &&
           left.line_text == right.line_text && left.text_begin == right.text_begin &&
           left.match_begin == right.match_begin && left.match_end == right.match_end;
}
} // namespace nenenib::core
