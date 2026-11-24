#pragma once

#include "AutosaveId.hpp"
#include "JsonValue.hpp"
#include "Language.hpp"
#include "SentRevision.hpp"

#include <string>
#include <vector>

namespace nenenib::application
{
// 言語サーバーに `didOpen` した文書 1 つ（ADR 0161 の決定 1）。tab はタブの退避番号（帯の位置・
// パスでは名指ししない）、uri は `didOpen` に添えた URI（届いた診断の文書を引く鍵）、sent は送った
// 版の列（古い順・末尾が最後に送った版・language_server_kept_revisions まで）、changes はこの意図の
// 中でまだ送っていない `TextDocumentContentChangeEvent` の列（apply の終わりに 1 通の `didChange`
// に束ねる）、language は `didOpen` したときの文書の言語（`languageId` の元・席を共有する c と cpp
// の間で `:set ft=` が変わったら閉じて開き直す・ADR 0161 の 2026-11-24 の追補）。公開 aggregate
// （CPP-003）。
struct LanguageServerDocument
{
    AutosaveId tab;
    std::string uri;
    std::vector<SentRevision> sent;
    core::JsonArray changes = {};
    core::Language language = core::Language::plain;
};
} // namespace nenenib::application
