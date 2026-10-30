#pragma once

namespace nenenib::application
{
// 「置換」の操作（Ctrl+H と F1 の一覧・ADR 0174 の決定 2・3・7）。通常モードは置換の流れの
// 「検索: 」を開き、Enter で「置換後: 」へ進む（1 件は Enter・全件は Ctrl+Enter）。Vim モードは
// `:` の入力行に `%s/` を前置きする。
struct BeginReplace
{
};
} // namespace nenenib::application
