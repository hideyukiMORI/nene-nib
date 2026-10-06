#pragma once

#include "OperationKey.hpp"

#include <string>

namespace nenenib::core
{
// 操作の鍵 1 つ（ADR 0078 の決定 2）。Ctrl と Shift の有無と鍵。Alt と組んだ鍵は ui が写さない。
struct KeyChord
{
    bool control;
    bool shift;
    OperationKey key;
};

// 等値は非メンバで書く（CPP-003）。
[[nodiscard]] constexpr bool operator==(const KeyChord &left, const KeyChord &right) noexcept
{
    return left.control == right.control && left.shift == right.shift && left.key == right.key;
}

// 鍵の表示名（`Ctrl+Shift+S`・`F1`・`Ctrl++`）。並びは Ctrl → Shift → 鍵で、表に文字列を書かない
// （ADR 0078 の決定 5）。
[[nodiscard]] std::string key_chord_label(KeyChord chord);
} // namespace nenenib::core
