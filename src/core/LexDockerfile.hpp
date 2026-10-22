#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// Dockerfile の 1 行の字句器（#860・ADR 0156 の追補）。入口は lex_line と
// lex_state で、ここを直接呼ばない。分類は Issue #860 の表（Vim の dockerfile.vim と
// Dockerfile reference に寄せる）で、命令と `AS` は ASCII の大小を問わない。
// 行をまたぐのは行末の `\` の続き（plain の delimiter `\`・続きの行の頭の語を命令に
// しない）とヒアドキュメント `<<EOF` `<<-EOF` `<<"EOF"`（raw_string の delimiter に
// 閉じの語・`<<-` は頭に `-`）。
void lex_dockerfile(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
