#pragma once

#include <cstdint>

namespace nenenib::core
{
// 入力行の種類の閉じた一覧（ADR 0032 の決定 1）。プロンプトの文字はこの値から決まり、
// 描画はどの入力行でも 1 本の経路を通る（ARC-001）。増えたら renderer の switch が落ちる。
enum class InputLinePrompt : std::uint8_t
{
    // Ex（`:`）。
    ex,
    // Ctrl+P の設定一覧。入力行にプロンプトの文字を出さない。
    palette,
    // `/` の検索。
    search_forward,
    // `?` の検索。
    search_backward,
    // 通常モードの検索（「検索: 」・ADR 0174 の決定 3）。語は literal で照合器へ渡る。
    find,
    // 通常モードの行へ移動（「行: 」・ADR 0174 の決定 3・8）。
    go_to_line,
    // 通常モードの置換の 2 段目（「置換後 (語): 」・ADR 0174 の決定 3・7）。置換後の語は literal
    // で、 照合器へは渡らない。
    replace_with,
    // `:s///c` の問い（ADR 0168 の決定 6）。プロンプトの文字を出さず、文は Vim の
    // `replace with … (y/n/a/q/l)?`。文字は答えとしてだけ読む。
    substitute_confirm,
    // イージーモーションの `\\f` `\\F` `\\s` の 1 文字の入力行（ADR 0163 の決定 4）。確定した最初の
    // 1 文字を跳び先の文字として engine へ流す。
    jump_character
};
} // namespace nenenib::core
