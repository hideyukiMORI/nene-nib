// 軽い反映値と完全な描画値の共通契約（ADR 0084）。製品と同じ apply を直接通す。
#include "AdjustFontSize.hpp"
#include "CancelCommand.hpp"
#include "CancelComposition.hpp"
#include "CloseTab.hpp"
#include "CommandText.hpp"
#include "ComposeText.hpp"
#include "Editing.hpp"
#include "EditorDelivery.hpp"
#include "FontSizeAdjustment.hpp"
#include "InsertText.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "OpenOperationList.hpp"
#include "RefreshAppearance.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SettingsFailure.hpp"
#include "SubmitCommand.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <concepts>
#include <expected>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

static_assert(std::same_as<decltype(std::declval<app::EditorController &>().apply(
                               std::declval<const app::EditorIntent &>())),
                           app::EditorDelivery>);
static_assert(std::same_as<decltype(std::declval<app::EditorController &>().apply_frame(
                               std::declval<const app::EditorIntent &>())),
                           app::EditorFrame>);

[[nodiscard]] bool same_composition(const std::optional<app::CompositionView> &left,
                                    const std::optional<app::CompositionView> &right)
{
    if (left.has_value() != right.has_value())
    {
        return false;
    }
    return !left.has_value() || (left.value().utf8 == right.value().utf8 &&
                                 left.value().underlines == right.value().underlines &&
                                 left.value().cursor == right.value().cursor);
}

[[nodiscard]] auto document_values(const app::DocumentView &document)
{
    return std::tuple{std::string(document.title.text()), document.path, document.encoding,
                      document.save_state, document.last_failure};
}

[[nodiscard]] auto message_values(const std::optional<core::DisplayText> &message)
{
    return message.has_value() ? std::optional{std::string(message.value().text())} : std::nullopt;
}

void expect_common(const app::EditorDelivery &delivered, const app::EditorFrame &frame)
{
    expect(delivered.appearance == frame.appearance && delivered.mode == frame.mode &&
               delivered.vim_mode == frame.vim_mode && delivered.ime == frame.ime,
           "delivery and frame agree on appearance, modes and IME stance");
    expect(same_composition(delivered.composition, frame.composition) &&
               same_composition(delivered.command_composition, frame.command_composition),
           "delivery and frame agree on both composition snapshots");
    expect(document_values(delivered.document) == document_values(frame.document),
           "delivery and frame agree on document and one-intent file failure");
    expect(core::same_settings(delivered.settings, frame.settings) &&
               delivered.settings_failure == frame.settings_failure &&
               message_values(delivered.command_message) == message_values(frame.command_message),
           "delivery and frame agree on settings, failure and message");
    expect(delivered.closing == frame.closing && delivered.close_request == frame.close_request &&
               delivered.operation_request == frame.operation_request,
           "delivery and frame agree on all one-intent requests");
}

[[nodiscard]] app::EditorDelivery checked_apply(app::EditorController &controller,
                                                const app::EditorIntent &intent)
{
    auto delivered = controller.apply(intent);
    const auto frame = controller.frame();
    expect_common(delivered, frame);
    expect(controller.is_composing() == app::composing(frame),
           "light composition query agrees with either frame composition");
    const auto documents = controller.documents();
    expect(documents.size() == frame.tabs.size(), "light documents query has all frame tabs");
    for (std::size_t index = 0; index < documents.size(); ++index)
    {
        expect(document_values(documents.at(index)) == document_values(frame.tabs.at(index)),
               "light documents query agrees on each tab");
    }
    return delivered;
}

void verify_delivery_composition()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(checked_apply(controller, app::VisibleLines{3}));
    static_cast<void>(checked_apply(controller, app::InsertText{"body\nnext"}));
    const auto composed = checked_apply(controller, app::ComposeText{composed_of("ime", {}, 1)});
    expect(composed.composition.has_value() && !composed.command_composition.has_value(),
           "body composition is delivered without command composition");
    static_cast<void>(checked_apply(controller, app::CancelComposition{}));
    static_cast<void>(checked_apply(controller, app::OpenCommandPalette{}));
    const auto command = checked_apply(controller, app::ComposeText{composed_of("query", {}, 2)});
    expect(!command.composition.has_value() && command.command_composition.has_value(),
           "palette composition is delivered without body composition");
    const auto palette = controller.command_palette_view();
    const auto frame = controller.frame();
    expect(palette.has_value() && frame.command_palette.has_value() &&
               palette.value().selected == frame.command_palette.value().selected &&
               palette.value().total == frame.command_palette.value().total,
           "public palette query shares the frame candidate calculation");
    static_cast<void>(checked_apply(controller, app::CancelCommand{}));
    expect(command.command_composition.has_value() && composed.composition.has_value() &&
               command.command_composition.value().utf8 == "query" &&
               composed.composition.value().utf8 == "ime",
           "earlier delivery owns composition bytes after later intents");
}

[[nodiscard]] app::EditorDelivery checked_ex(app::EditorController &controller, std::string text)
{
    static_cast<void>(checked_apply(controller, app::VimKeyPress{core::VimCharacter{U':'}}));
    static_cast<void>(checked_apply(controller, app::CommandText{std::move(text)}));
    return checked_apply(controller, app::SubmitCommand{});
}

void verify_delivery_requests()
{
    Editing editing;
    auto &controller = editing.controller();
    const auto failed = checked_apply(controller, app::OpenDocument{sample_path()});
    expect(failed.document.last_failure == app::FileFailure::not_found,
           "failed open delivers the file failure immediately");
    const auto next = checked_apply(controller, app::VisibleLines{4});
    expect(!next.document.last_failure.has_value() && failed.document.last_failure.has_value(),
           "next intent clears current failure without changing owned prior delivery");
    static_cast<void>(checked_apply(controller, app::SelectEditMode{core::EditMode::vim}));
    const auto close = checked_ex(controller, "tabclose");
    expect(close.close_request == 0, "Ex close delivers the requested tab immediately");
    expect(!checked_apply(controller, app::VisibleLines{4}).close_request.has_value() &&
               close.close_request == 0,
           "close request lives for one intent and snapshot survives");
    static_cast<void>(checked_apply(controller, app::OpenOperationList{}));
    static_cast<void>(checked_apply(controller, app::CommandText{"ほぞん"}));
    const auto operation = checked_apply(controller, app::SubmitCommand{});
    expect(operation.operation_request.has_value(), "operation command delivers its request");
    expect(!checked_apply(controller, app::VisibleLines{4}).operation_request.has_value(),
           "next intent clears the operation request");
    const auto closing = checked_apply(controller, app::CloseTab{0});
    expect(closing.closing && !checked_apply(controller, app::VisibleLines{4}).closing,
           "last-tab closing is delivered for exactly one intent");
}

void verify_delivery_settings_and_worker()
{
    Editing editing(std::unexpected(app::SettingsIssue{app::SettingsFailure::malformed}));
    auto &controller = editing.controller();
    const auto initial = controller.delivery();
    expect_common(initial, controller.frame());
    expect(initial.settings_failure.has_value(), "startup settings failure has a light snapshot");
    const auto changed =
        checked_apply(controller, app::AdjustFontSize{core::FontSizeAdjustment::increase, 1});
    expect(changed.settings.font_size.points() > initial.settings.font_size.points() &&
               !changed.settings_failure.has_value(),
           "font update delivers persisted settings");
    editing.appearance().script(core::Appearance::light);
    const auto appeared = checked_apply(controller, app::RefreshAppearance{});
    expect(appeared.appearance == core::Appearance::light,
           "light result follows refreshed appearance");
    static_cast<void>(checked_apply(controller, app::NewTab{}));
    static_cast<void>(checked_apply(controller, app::WalkRecentTab{core::TabStep::next}));
    expect(controller.tab_walking(), "recent-tab walk starts on product apply path");
    const auto before = controller.delivery();
    const auto collected = checked_apply(controller, app::WorkCompleted{});
    expect(controller.tab_walking() &&
               document_values(before.document) == document_values(collected.document),
           "worker completion preserves the active document and MRU walk");
    expect(editing.folders().collects() == 1,
           "worker completion collects through the existing port");
    const auto wrapped = controller.apply_frame(app::VisibleLines{4});
    expect_common(controller.delivery(), wrapped);
}
} // namespace

void verify_delivery_contracts()
{
    verify_delivery_composition();
    verify_delivery_requests();
    verify_delivery_settings_and_worker();
}
} // namespace nenenib::tests
