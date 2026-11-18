#pragma once

#include <string>
#include <vector>

namespace nenenib::application
{
// `initialize` の答えの `capabilities` から読んだサーバーの能力（ADR 0190 の決定 3）。completion
// は `completionProvider` があること、trigger_characters はその `triggerCharacters`（文字列だけ・
// 届いた順）、hover と definition は `hoverProvider` `definitionProvider` が true か object で
// あること。持たない能力の要求は送らない。公開 aggregate（CPP-003）。
struct ServerAbilities
{
    bool completion = false;
    std::vector<std::string> trigger_characters = {};
    bool hover = false;
    bool definition = false;
};
} // namespace nenenib::application
