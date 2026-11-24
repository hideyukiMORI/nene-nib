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
    // `referencesProvider`（#1947・ADR 0191 の決定 2）。
    bool references = false;
    // `documentSymbolProvider`（#1955・ADR 0191 の決定 2）。
    bool document_symbols = false;
    // `signatureHelpProvider` と、その `triggerCharacters` `retriggerCharacters`（#1959・ADR 0191
    // の 決定 2・6・文字列だけを届いた順に）。
    bool signature_help = false;
    std::vector<std::string> signature_triggers = {};
    std::vector<std::string> signature_retriggers = {};
    // `documentFormattingProvider` `documentRangeFormattingProvider`（#1993・ADR 0191 の決定
    // 2・7）。
    bool formatting = false;
    bool range_formatting = false;
    // `renameProvider`（true か object）と、その `prepareProvider`（#2004・ADR 0194 の決定 3）。
    bool rename = false;
    bool prepare_rename = false;
    // `codeActionProvider`（true か object）と、その `resolveProvider`、`executeCommandProvider` の
    // `commands`（文字列だけを届いた順に・#2005・ADR 0194 の決定 3）。
    bool code_action = false;
    bool code_action_resolve = false;
    std::vector<std::string> execute_commands = {};
    DocumentSync sync = DocumentSync::none;
    SaveNotice save = SaveNotice::none;
};
} // namespace nenenib::application
