#pragma once

#include <cstdint>

namespace nenenib::application
{
// 言語サーバーの編集（WorkspaceEdit）がどこから来たか（#2005・ADR 0194 の決定 4 の (a)）。answer は
// Nib が頼んだ要求の答え（名前の変更・クイックフィックスの edit と
// resolve）で、頼んだ文書と改訂番号 を照らす。server_request はサーバーからの `workspace/applyEdit`
// で、頼んだ文書が無いので (a) を 飛ばし、`workspace/executeCommand` を送ったときの写しで (b)(c)
// だけを照らす。
enum class ServerEditOrigin : std::uint8_t
{
    answer,
    server_request
};
} // namespace nenenib::application
