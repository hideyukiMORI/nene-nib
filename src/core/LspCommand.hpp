#pragma once

#include "JsonValue.hpp"

#include <optional>
#include <string>

namespace nenenib::core
{
// 言語サーバーの `Command`（#2005・ADR 0194 の決定 8・10）。title は見せる題名、command は
// `workspace/executeCommand` に渡す名前、arguments はサーバーが添えた引数の配列の JSON のまま
// （無ければ値なし）。公開 aggregate（CPP-003）。
struct LspCommand
{
    std::string title;
    std::string command;
    std::optional<JsonValue> arguments;
};
} // namespace nenenib::core
