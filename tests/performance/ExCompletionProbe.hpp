#pragma once
#include "ExCompletionSample.hpp"
#include "ExCompletionWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace nenenib::tests::performance
{
[[nodiscard]] std::string ex_completion_input(ExCompletionWorkload workload);
[[nodiscard]] const ExCompletionSample &ex_completion_sample(ExCompletionWorkload workload);
// 組み込みテーマの名前を名前順で（数も名前も core::builtin_themes から引く・#690。並びは
// ThemeCatalog::names と同じ core::ascii_folded_before・#696）。
[[nodiscard]] std::vector<std::string> builtin_theme_names();
// 組み込みと custom の名前を 1 つの名前順に混ぜた列（ThemeCatalog::names の期待・#696）。
[[nodiscard]] std::vector<std::string> theme_names_with(std::span<const std::string_view> custom);
// 利用者テーマの 256 個の名前（user-0000〜user-0255）。
[[nodiscard]] std::span<const std::string_view> custom_theme_names();
// prefix の補完の期待（nullopt は全件の一覧）。固定の候補の後ろに theme_names のテーマと system。
// テーマの引数は ASCII の大文字小文字を無視して照合する（命令名は区別・#696）。
[[nodiscard]] std::vector<std::string>
expected_ex_completions(std::optional<std::string_view> prefix,
                        std::span<const std::string> theme_names);
[[nodiscard]] std::vector<std::string> expected_completions(ExCompletionWorkload workload);
[[nodiscard]] std::uint64_t completion_checksum(std::span<const std::string> candidates);
[[nodiscard]] bool completions_equal(std::span<const std::string> candidates,
                                     std::span<const std::string> expected);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
probed_ex_completion(ExCompletionWorkload workload, const std::string &input,
                     application::TimingPort &timing);
} // namespace nenenib::tests::performance
