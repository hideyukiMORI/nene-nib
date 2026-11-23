#pragma once

#include "LanguageCompletion.hpp"
#include "LanguageDefinition.hpp"
#include "LanguageHover.hpp"
#include "LanguageSymbols.hpp"

#include <optional>

namespace nenenib::application
{
// 言語サーバーの答えのうち、文脈の照合を通って状態に置いた最新のもの（ADR 0190 の決定 1）。
// 種類ごとに最後に届いた 1 つで、置くのは controller の答えの振り分けの 1 か所だけ。面・札・
// 跳ぶ道（段 B / C）が読んで消す。公開 aggregate（CPP-003）。
struct LanguageAnswers
{
    std::optional<LanguageCompletion> completion = std::nullopt;
    std::optional<LanguageHover> hover = std::nullopt;
    std::optional<LanguageDefinition> definition = std::nullopt;
    // 参照の答え（#1947・ADR 0191 の決定 4）。形は定義と同じ場所の列。
    std::optional<LanguageDefinition> references = std::nullopt;
    // 記号の答え（#1955・ADR 0191 の決定 5）。
    std::optional<LanguageSymbols> symbols = std::nullopt;
};
} // namespace nenenib::application
