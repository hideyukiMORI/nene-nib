#pragma once

#include "ShellRegistrationInput.hpp"
#include "ShellRegistrationPlan.hpp"
#include "ShellRegistrationPlanFailure.hpp"

#include <expected>
#include <string>
#include <vector>

namespace nenenib::core
{
// Windows のエクスプローラーへの登録の鍵と値は core のこの 1 か所が作る（ADR 0178 の決定 2）。
// 書く・消す・確かめるのは application の ShellRegistrationPort（決定 1）。

// 鍵と値を書く順に並べる。右クリックの項目（`Software\Classes\*\shell\NeNeNib`）・「プログラム
// から開く」（`Software\Classes\Applications\NeNeNib.exe`）・拡張子ごとの ProgId
// （`Software\Classes\NeNeNib.<拡張子>`）・「既定のアプリ」（`Software\NeNeNib\Capabilities` と
// `Software\RegisteredApplications`）の順。exe のパスと `%1` は `"` で囲み、拡張子は小文字・`.`
// 付き・重複なしに畳む。パスが空か `"` を含めば失敗（ARC-010）。
[[nodiscard]] std::expected<ShellRegistrationPlan, ShellRegistrationPlanFailure>
shell_registration_plan(const ShellRegistrationInput &input);

// 外すときに木ごと消す根（removal が subtree の値の木）を計画の順に重複なく並べる。右クリックの
// 項目・アプリ・拡張子ごとの ProgId・`Software\NeNeNib` の順。共有の鍵の値（value）は含めない。
[[nodiscard]] std::vector<std::string>
shell_registration_subtrees(const ShellRegistrationPlan &plan);

// 登録する拡張子。文書の言語の表（ADR 0154・listed_language_extensions）の拡張子の全部と `.txt`
// を表の順に、小文字・`.` 付き・重複なしで並べる（別の表を書かない）。
[[nodiscard]] std::vector<std::string> shell_registration_extensions();
} // namespace nenenib::core
