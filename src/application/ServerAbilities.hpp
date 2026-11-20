#pragma once

#include "DocumentSync.hpp"
#include "SaveNotice.hpp"

#include <string>
#include <vector>

namespace nenenib::application
{
// `initialize` の答えの `capabilities` から読んだサーバーの能力（ADR 0190 の決定 3）。completion
// は `completionProvider` があること、trigger_characters はその `triggerCharacters`（文字列だけ・
// 届いた順）、hover と definition は `hoverProvider` `definitionProvider` が true か object で
// あること。持たない能力の要求は送らない。sync と save は `textDocumentSync` の種類と `save`
// （ADR 0161 の 2026-11-20 の追補）。公開 aggregate（CPP-003）。
struct ServerAbilities
{
    bool completion = false;
    std::vector<std::string> trigger_characters = {};
    bool hover = false;
    bool definition = false;
    DocumentSync sync = DocumentSync::none;
    SaveNotice save = SaveNotice::none;
};
} // namespace nenenib::application
