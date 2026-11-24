#pragma once

#include "LspCodeAction.hpp"
#include "LspEditContext.hpp"
#include "LspPositionEncoding.hpp"

#include <string>
#include <vector>

namespace nenenib::application
{
// クイックフィックスの答え（#2005・ADR 0194 の決定 9）。context は頼んだときの文脈と席の文書の
// 改訂番号の写し（選んで当てるときに照らす・頼んだ席は context.slot）、actions はサーバーの順の
// 項目（読めない答えは空）、server_error は誤りの答えの message（誤りでなければ空）、encoding は
// 答えた席の位置の単位。公開 aggregate（CPP-003）。
struct LanguageCodeActions
{
    LspEditContext context;
    std::vector<core::LspCodeAction> actions;
    std::string server_error;
    core::LspPositionEncoding encoding;
};
} // namespace nenenib::application
