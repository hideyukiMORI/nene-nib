#pragma once

#include "DisplayText.hpp"
#include "EditorSettings.hpp"
#include "ExDocumentRequest.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ExPaletteRequest.hpp"
#include "ExTabRequest.hpp"
#include "ThemeCatalog.hpp"
#include "VimSearchHighlight.hpp"

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// Ex の 1 命令の答え（ADR 0022）。settings は保存する設定、highlight は検索の当たりの強調、
// incsearch は入力中の当たりの preview の有無（ADR 0041 の決定 6）、tab はタブの命令（ADR 0057 の
// 決定 4）で、どれも「変わらない」ことを空で表す。写し先は controller の Ex の経路 1 か所だけ
// （ARC-004）。タブの命令の message は命令の完全な名前で、controller は画面に出さない（Vim も
// 成功した切り替えでは何も出さない）。palette は共通の一覧を開く要求（ADR 0064）。
// document は現在の文書への保存・終了（ADR 0066）。
struct ExResult
{
    std::optional<EditorSettings> settings;
    std::optional<VimSearchHighlight> highlight;
    std::optional<bool> incsearch;
    DisplayText message;
    std::optional<ExTabRequest> tab = std::nullopt;
    std::optional<ExPaletteRequest> palette = std::nullopt;
    std::optional<ExDocumentRequest> document = std::nullopt;
};

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure>
evaluate_ex(std::string_view text, const EditorSettings &settings, Appearance system_appearance,
            const ThemeCatalog &themes = ThemeCatalog::builtins());
[[nodiscard]] std::vector<std::string>
command_completions(std::string_view prefix, const ThemeCatalog &themes = ThemeCatalog::builtins());
[[nodiscard]] std::vector<std::string>
ex_command_candidates(const ThemeCatalog &themes = ThemeCatalog::builtins());
} // namespace nenenib::core
