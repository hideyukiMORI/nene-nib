#pragma once

#include "Appearance.hpp"
#include "CompositionView.hpp"
#include "DocumentView.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "ImeStance.hpp"
#include "SettingsIssue.hpp"
#include "VimMode.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// 意図を直ちに写した所有スナップショット。本文の行や描画の仕事は含まない（ADR 0084）。
struct EditorDelivery
{
    core::Appearance appearance;
    core::EditMode mode;
    core::VimMode vim_mode;
    ImeStance ime;
    std::optional<CompositionView> composition;
    std::optional<CompositionView> command_composition;
    DocumentView document;
    core::EditorSettings settings;
    std::optional<SettingsIssue> settings_failure;
    std::optional<core::DisplayText> command_message;
    bool closing;
    std::optional<std::size_t> close_request;
    std::optional<core::EditorOperation> operation_request;
};
} // namespace nenenib::application
