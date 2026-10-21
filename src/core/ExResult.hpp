#pragma once

#include "DisplayForm.hpp"
#include "DisplayText.hpp"
#include "EditorSettings.hpp"
#include "ExDocumentRequest.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ExLanguageRequest.hpp"
#include "ExPaletteRequest.hpp"
#include "ExSyntaxRequest.hpp"
#include "ExTabRequest.hpp"
#include "ExWindowCommand.hpp"
#include "FontFamilies.hpp"
#include "ThemeCatalog.hpp"
#include "VimSearchHighlight.hpp"

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 字体を変える Ex の命令の頭。補完・候補・字体の一覧を読む時機はこの 1
// つの文字列から決める（#597）。
inline constexpr std::string_view guifont_command = "set guifont=";

// Ex の 1 命令の答え（ADR 0022）。settings は保存する設定、highlight は検索の当たりの強調、
// incsearch は入力中の当たりの preview の有無（ADR 0041 の決定 6）、tab はタブの命令（ADR 0057 の
// 決定 4）で、どれも「変わらない」ことを空で表す。写し先は controller の Ex の経路 1 か所だけ
// （ARC-004）。タブの命令の message は命令の完全な名前で、controller は画面に出さない（Vim も
// 成功した切り替えでは何も出さない）。palette は共通の一覧を開く要求（ADR 0064）。
// document は現在の文書への保存・終了（ADR 0066）。display は現在の文書の表示形式（ADR 0140 の
// 決定 3・設定に保存しない）。language は現在の文書の言語の選択か問い（ADR 0154 の決定 4・設定にも
// セッションにも保存しない）。syntax は現在の文書の構文の色の切り替えか問い（ADR 0156 の追補・
// 設定にもセッションにも保存しない）。
struct ExResult
{
    std::optional<EditorSettings> settings;
    std::optional<VimSearchHighlight> highlight;
    std::optional<bool> incsearch;
    DisplayText message;
    std::optional<ExTabRequest> tab = std::nullopt;
    std::optional<ExPaletteRequest> palette = std::nullopt;
    std::optional<ExDocumentRequest> document = std::nullopt;
    std::optional<DisplayForm> display = std::nullopt;
    std::optional<ExWindowCommand> window = std::nullopt;
    std::optional<ExLanguageRequest> language = std::nullopt;
    std::optional<ExSyntaxRequest> syntax = std::nullopt;
};

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure>
evaluate_ex(std::string_view text, const EditorSettings &settings, Appearance system_appearance,
            const ThemeCatalog &themes = ThemeCatalog::builtins());
// prefix が `set guifont=` で始まるときだけ、fonts の名前を `set guifont=<名前>`
// の候補として後ろに足す （施主決定 D58）。`set filetype=` `set ft=` で始まるときは言語の名前を
// 同じ形で足す（ADR 0154 の決定 4）。
[[nodiscard]] std::vector<std::string>
command_completions(std::string_view prefix, const ThemeCatalog &themes = ThemeCatalog::builtins(),
                    const FontFamilies &fonts = FontFamilies::none());
[[nodiscard]] std::vector<std::string>
ex_command_candidates(const ThemeCatalog &themes = ThemeCatalog::builtins());
} // namespace nenenib::core
