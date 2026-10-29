#pragma once

namespace nenenib::application
{
// 「置換」の操作（Ctrl+H と F1 の一覧・ADR 0174 の決定 3・7）。本体（「検索: 」→
// 「置換後: 」の 2 段と全件の 1 回の Edit）は #1046 の N1b が書く。いまは
// 「置換は後続です」の 1 行を知らせるだけ。
struct BeginReplace
{
};
} // namespace nenenib::application
