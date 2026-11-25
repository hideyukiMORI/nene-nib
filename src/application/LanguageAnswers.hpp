#pragma once

#include "LanguageCodeActionResolve.hpp"
#include "LanguageCodeActions.hpp"
#include "LanguageCompletion.hpp"
#include "LanguageDefinition.hpp"
#include "LanguageFormatting.hpp"
#include "LanguageHover.hpp"
#include "LanguageRename.hpp"
#include "LanguageRenameTarget.hpp"
#include "LanguageSignature.hpp"
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
    // 引数の情報の答え（#1959・ADR 0191 の決定 6）。札の欄は controller の signature_tip_。
    std::optional<LanguageSignature> signature = std::nullopt;
    // 整形の答え（#1993・ADR 0191 の決定 7）。照合は当てるときにする（変わっていたら 1
    // 行知らせる）。
    std::optional<LanguageFormatting> formatting = std::nullopt;
    // 名前の変更の答え（#2004・ADR 0194 の決定 7）。変えられる所の問いの答えと、名前の変更の答え。
    // 照合はどちらも使うときにする。
    std::optional<LanguageRenameTarget> rename_target = std::nullopt;
    std::optional<LanguageRename> rename = std::nullopt;
    // クイックフィックスの答えと、`codeAction/resolve` の答え（#2005・ADR 0194 の決定 9・10）。
    std::optional<LanguageCodeActions> code_actions = std::nullopt;
    std::optional<LanguageCodeActionResolve> code_action_resolve = std::nullopt;
};
} // namespace nenenib::application
