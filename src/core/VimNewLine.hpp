#pragma once

namespace nenenib::core
{
// INSERT の Enter。入る文字列は本文が持つ改行の形が決めるので、core は「改行」とだけ言う
// （ARC-009 / ADR 0009 の決定 8）。文字列で運ぶと改行の正典が 2 つになる（ARC-001）。
struct VimNewLine
{
};
} // namespace nenenib::core
