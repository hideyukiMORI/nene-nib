#pragma once

#include <cstdint>

namespace nenenib::core
{
// parse_json が断る入力の閉じた一覧（RFC 8259・ADR 0160 の決定 4）。期待される失敗なので例外に
// しない（CPP-005）。
enum class JsonFailure : std::uint8_t
{
    // 値の途中で入力が尽きた（空の入力・閉じていない文字列や括弧）
    unexpected_end,
    // 値・`,`・`:`・`]`・`}`・鍵の来る所に来ない文字
    unexpected_character,
    // 値の後ろに空白以外のバイトが残った
    trailing_characters,
    // `true` `false` `null` の綴りの誤り
    invalid_literal,
    // 数の文法の誤り（`01` `1.` `-` `-a` `1e`）。`+1` と `.5` は値の始まりでないので
    // unexpected_character
    invalid_number,
    // double の範囲を外れる数（`1e400`）
    number_out_of_range,
    // 文字列の中の未知の `\` の列
    invalid_escape,
    // `\u` の後が 16 進 4 桁でない
    invalid_unicode_escape,
    // 対になっていない代理（`\uD800` だけ・`\uDC00` から始まる）
    unpaired_surrogate,
    // 文字列の中の生の制御文字（U+0000〜U+001F）
    control_character,
    // 正しい UTF-8 でない入力
    invalid_utf8,
    // 配列と object の入れ子が 128 を超えた
    too_deep
};
} // namespace nenenib::core
