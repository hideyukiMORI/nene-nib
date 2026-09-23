// 単体テストの基盤（ADR 0042 決定 1）。計数と expect、Vim 以外にも使う足場の宣言。
#pragma once

#include "CommandChoice.hpp"
#include "Composition.hpp"
#include "CompositionClause.hpp"
#include "DisplayText.hpp"
#include "Edit.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "FilePath.hpp"
#include "TextBuffer.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
std::size_t &failure_count();
std::size_t &check_count();
void expect(bool condition, const char *description);
nenenib::core::DisplayText fixed_text(std::string_view text);
nenenib::core::TextBuffer buffer_of(std::string_view text);
[[nodiscard]] nenenib::core::Edit absorbed_edit(const nenenib::core::Edit &previous,
                                                const nenenib::core::Edit &edit);
std::string applied(nenenib::application::EditorController &controller,
                    const nenenib::application::EditorIntent &intent);
nenenib::core::FilePath sample_path();
[[nodiscard]] nenenib::core::Composition
composed_of(std::string utf8, std::vector<nenenib::core::CompositionClause> clauses,
            std::size_t cursor);
nenenib::application::EditorFrame run_ex(nenenib::application::EditorController &controller,
                                         std::string text);
std::vector<nenenib::core::CommandChoice> choices_for(std::string_view query);
} // namespace nenenib::tests
