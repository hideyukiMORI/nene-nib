#pragma once

#include <string>

namespace nenenib::application
{
// 子の標準エラーから読んだバイト列（ADR 0160 の 2026-11-22 の追補・LSP 段 S8）。行の区切りも
// UTF-8 の検証もしていない生の塊で、行に分けて直すのは controller の記録（LanguageServerLog）。
// adapter はこの出来事だけでは届いたの合図を出さない（次の本文か子の終わりと一緒に届く）。公開
// aggregate（CPP-003）。
struct ServerErrorOutput
{
    std::string bytes;
};
} // namespace nenenib::application
