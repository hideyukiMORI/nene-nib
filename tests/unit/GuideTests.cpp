// Issue #304 / ADR 0079。案内の文脈・操作表の表示値・排他的な配置だけを検証する。
#include "BodyGuideLayout.hpp"
#include "BodyLayout.hpp"
#include "CancelCommand.hpp"
#include "CancelComposition.hpp"
#include "CommandChoice.hpp"
#include "ComposeText.hpp"
#include "DevicePixels.hpp"
#include "Editing.hpp"
#include "GuideContext.hpp"
#include "HistoryAction.hpp"
#include "InsertText.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "OpenOperationList.hpp"
#include "OperationBindings.hpp"
#include "OperationGuide.hpp"
#include "OperationGuideLayout.hpp"
#include "OperationTexts.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SettingsFailure.hpp"
#include "StatusBarLayout.hpp"
#include "StatusGuideLayout.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "VimCharacter.hpp"
#include "VimSpecialKey.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using core::GuideContext;

void expect_context(const app::EditorFrame &frame, GuideContext context)
{
    expect(frame.guide.context == context, "frame carries the authoritative guide context");
    if (context == GuideContext::hidden)
    {
        for (const auto &entry : frame.guide.entries)
        {
            expect(entry.key.empty() && entry.body_name.empty() && entry.short_name.empty(),
                   "hidden guide builds no labels or keys");
        }
    }
}

void verify_guide_text()
{
    for (const auto mode : {core::EditMode::ordinary, core::EditMode::vim})
    {
        const auto guide = core::operation_guide(GuideContext::untouched_untitled, mode);
        constexpr std::array<core::EditorOperation, 3> operations{
            core::EditorOperation::list_files, core::EditorOperation::open_file,
            core::EditorOperation::list_operations};
        for (std::size_t index = 0; index < operations.size(); ++index)
        {
            const auto operation = operations.at(index);
            const auto chord = core::shown_chord(operation, mode);
            expect(chord.has_value(), "each guide operation has a key in both editing modes");
            expect(guide.entries.at(index).key ==
                       core::key_chord_label(
                           chord.value_or(core::KeyChord{false, false, core::OperationKey::f1})),
                   "guide key comes from the canonical operation binding");
        }
        expect(guide.entries.at(0).body_name == "ファイルとタブの一覧" &&
                   guide.entries.at(0).short_name == "一覧",
               "files guide uses the full body name and the short status name");
        expect(guide.entries.at(1).body_name == "ファイルを開く" &&
                   guide.entries.at(1).short_name == guide.entries.at(1).body_name,
               "missing short name falls back to the canonical name");
        expect(guide.entries.at(2).body_name == "ヘルプ" &&
                   guide.entries.at(2).short_name == "ヘルプ",
               "operation list is called help in both guides");
        expect(core::operation_choices("ヘルプ", mode).empty(),
               "new guide short names do not become operation list search targets");
    }
}

void verify_document_context()
{
    Editing editor;
    auto &controller = editor.controller();
    expect_context(controller.frame(), GuideContext::untouched_untitled);
    expect_context(controller.apply_frame(app::InsertText{"a"}), GuideContext::other);
    const auto undone = controller.apply_frame(app::HistoryAction{core::HistoryDirection::undo});
    expect(undone.lines.front().text.empty(), "undo returned the document to zero bytes");
    expect_context(undone, GuideContext::other);
    expect_context(controller.apply_frame(app::NewTab{}), GuideContext::untouched_untitled);
    expect_context(controller.apply_frame(app::SwitchTab{0}), GuideContext::other);
    expect_context(controller.apply_frame(app::SwitchTab{1}), GuideContext::untouched_untitled);
    editor.files().hold(std::string{});
    const auto opened = controller.apply_frame(app::OpenDocument{sample_path()});
    expect(opened.document.path.has_value() && opened.lines.front().text.empty(),
           "opened file is named and empty");
    expect_context(opened, GuideContext::other);
    expect_context(controller.apply_frame(app::NewTab{}), GuideContext::untouched_untitled);
}

void verify_sessions_and_ime()
{
    Editing editor;
    auto &controller = editor.controller();
    expect_context(controller.apply_frame(app::ComposeText{composed_of("あ", {}, 0)}),
                   GuideContext::hidden);
    expect_context(controller.apply_frame(app::CancelComposition{}),
                   GuideContext::untouched_untitled);
    expect_context(controller.apply_frame(app::OpenCommandPalette{}), GuideContext::hidden);
    const auto composed = controller.apply_frame(app::ComposeText{composed_of("あ", {}, 0)});
    expect(composed.command_composition.has_value(), "palette IME uses the command composition");
    expect_context(composed, GuideContext::hidden);
    expect_context(controller.apply_frame(app::CancelCommand{}), GuideContext::untouched_untitled);
    expect_context(controller.apply_frame(app::OpenOperationList{}), GuideContext::hidden);
    static_cast<void>(controller.apply_frame(app::CancelCommand{}));
    expect_context(controller.apply_frame(app::SelectEditMode{core::EditMode::vim}),
                   GuideContext::untouched_untitled);
    expect_context(controller.press_vim_key(core::VimCharacter{U':'}), GuideContext::hidden);
    static_cast<void>(controller.apply_frame(app::CancelCommand{}));
    expect_context(controller.press_vim_key(core::VimCharacter{U'/'}), GuideContext::hidden);
    static_cast<void>(controller.apply_frame(app::CancelCommand{}));
    expect_context(controller.press_vim_key(core::VimCharacter{U'i'}),
                   GuideContext::untouched_untitled);
    expect_context(controller.apply_frame(app::ComposeText{composed_of("あ", {}, 0)}),
                   GuideContext::hidden);
    static_cast<void>(controller.apply_frame(app::CancelComposition{}));
    expect_context(controller.press_vim_key(core::VimCharacter{U'a'}), GuideContext::other);
}

void verify_recording_and_notice()
{
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply_frame(app::SelectEditMode{core::EditMode::vim}));
    static_cast<void>(controller.press_vim_key(core::VimCharacter{U'q'}));
    const auto recording = controller.press_vim_key(core::VimCharacter{U'a'});
    expect(recording.recording.has_value(), "recording starts before guide suppression check");
    expect_context(recording, GuideContext::hidden);
    expect_context(controller.press_vim_key(core::VimCharacter{U'q'}),
                   GuideContext::untouched_untitled);
    const auto notice = run_ex(controller, "unknown");
    expect(notice.command_message.has_value(), "invalid command creates a one-line notice");
    expect_context(notice, GuideContext::hidden);
    expect_context(controller.apply_frame(app::VisibleLines{12}), GuideContext::hidden);
    expect_context(controller.apply_frame(app::NewTab{}), GuideContext::untouched_untitled);
}

void verify_guide_setting_context()
{
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply_frame(app::SelectEditMode{core::EditMode::vim}));
    static_cast<void>(run_ex(controller, "set noguide"));
    expect_context(controller.apply_frame(app::NewTab{}), GuideContext::hidden);
    editor.settings().fail(app::SettingsFailure::unwritable);
    static_cast<void>(run_ex(controller, "set guide"));
    expect_context(controller.apply_frame(app::NewTab{}), GuideContext::hidden);
    editor.settings().fail(std::nullopt);
    const auto success = run_ex(controller, "set guide");
    expect_context(success, GuideContext::hidden);
    expect_context(controller.apply_frame(app::NewTab{}), GuideContext::untouched_untitled);
    auto saved = core::default_editor_settings();
    saved.guide = core::GuideVisibility::hidden;
    Editing restored{SettingsReading{saved}};
    expect_context(restored.controller().frame(), GuideContext::hidden);
}

void verify_body_geometry(const core::BodyGuideLayout &guide, const core::BodyLayout &body,
                          std::uint32_t dpi)
{
    const auto &first = guide.rows.front();
    expect(first.key.left >= body.band.left + core::to_pixels(16, dpi) &&
               first.label.right <= body.band.right - core::to_pixels(16, dpi),
           "body guide keeps both side margins");
    // 228 DIP は120 DPIで285 px。整数画素への中央寄せは左右1 pxの差までを許す。
    const auto center_difference =
        first.key.left + first.label.right - body.band.left - body.band.right;
    expect(center_difference >= -1 && center_difference <= 1,
           "body guide is centered on the full band within one physical pixel of rounding");
    expect(first.key.top ==
               std::max(body.band.top + core::height_of(body.band) / 3,
                        body.content.top + body.line_height + core::to_pixels(12, dpi)),
           "body top follows the band third and the first line clearance");
    expect(guide.note.bottom + core::to_pixels(12, dpi) <= body.band.bottom,
           "whole guide and bottom margin fit above status");
    for (const auto &row : guide.rows)
    {
        expect(row.key.right == first.key.right &&
                   row.label.left - row.key.right == core::to_pixels(20, dpi),
               "three keys share their right edge and have a fixed label gap");
        expect(core::width_of(row.key) == core::to_pixels(64, dpi) &&
                   core::width_of(row.label) == core::to_pixels(144, dpi),
               "body columns keep their fixed widths regardless of text font");
    }
    expect(guide.note.top - guide.rows.back().key.bottom == core::to_pixels(20, dpi),
           "footer keeps the adopted gap after the third row");
}

void verify_status_geometry(const core::StatusGuideLayout &guide,
                            const core::StatusBarLayout &status, std::uint32_t dpi)
{
    expect(status.items.front().left - guide.rows.back().label.right == core::to_pixels(28, dpi),
           "status guide keeps exactly 28 DIP before the right items");
    expect(guide.rows.front().key.left >= status.mode.right + core::to_pixels(16, dpi),
           "status guide leaves the mode and its clearance intact");
    expect(guide.rows.back().key.left - guide.rows.front().label.right == core::to_pixels(16, dpi),
           "two status pairs have the adopted separation");
    for (const auto &row : guide.rows)
    {
        expect(row.label.left - row.key.right == core::to_pixels(4, dpi),
               "fixed status key and label fields keep their gap");
    }
}

void verify_geometry_case(std::uint32_t dpi, std::int32_t width, std::int32_t height, float points)
{
    const auto size = core::FontSize::from_points(points).value();
    const auto body =
        core::body_layout(core::to_pixels(width, dpi), core::to_pixels(height, dpi), dpi, size);
    const auto status =
        core::status_bar_layout(core::to_pixels(width, dpi), core::to_pixels(height, dpi), dpi);
    const auto guide =
        core::operation_guide_layout(GuideContext::untouched_untitled, body, status, dpi);
    const std::size_t expected = height == 560 ? 1U : (width >= 646 ? 2U : 0U);
    expect(guide.index() == expected, "each tested window chooses exactly its eligible placement");
    if (const auto *layout = std::get_if<core::BodyGuideLayout>(&guide); layout != nullptr)
    {
        expect(height != 200, "minimum height cannot hold the whole body guide");
        verify_body_geometry(*layout, body, dpi);
    }
    if (const auto *layout = std::get_if<core::StatusGuideLayout>(&guide); layout != nullptr)
    {
        expect(width >= 646 && height == 200, "only a short wide window falls back to status");
        verify_status_geometry(*layout, status, dpi);
    }
    const auto other = core::operation_guide_layout(GuideContext::other, body, status, dpi);
    expect(!std::holds_alternative<core::BodyGuideLayout>(other),
           "named and edited documents never get a body guide");
    expect(std::holds_alternative<std::monostate>(other) == (width < 646),
           "360 and 640 DIP cannot reserve 160 DIP while preserving existing status items");
    expect(std::holds_alternative<std::monostate>(
               core::operation_guide_layout(GuideContext::hidden, body, status, dpi)),
           "hidden context remains hidden at every geometry");
}

void verify_geometry_at_dpi(std::uint32_t dpi)
{
    for (const auto width : {360, 640, 960})
    {
        for (const auto height : {200, 560})
        {
            verify_geometry_case(dpi, width, height, 8.0F);
            verify_geometry_case(dpi, width, height, 40.0F);
        }
    }
}

void verify_geometry()
{
    for (const auto dpi : {96U, 120U, 192U})
    {
        verify_geometry_at_dpi(dpi);
    }
    const auto body = core::body_layout(228 + 31, 560, 96, core::default_font_size());
    const auto status = core::status_bar_layout(228 + 31, 560, 96);
    expect(std::holds_alternative<std::monostate>(
               core::operation_guide_layout(GuideContext::untouched_untitled, body, status, 96)),
           "one pixel short of the horizontal body margins hides both guides");
    const auto exact = core::body_layout(228 + 32, 560, 96, core::default_font_size());
    expect(std::holds_alternative<core::BodyGuideLayout>(
               core::operation_guide_layout(GuideContext::untouched_untitled, exact, status, 96)),
           "exact horizontal body margins admit the body guide");
}

void verify_layout_boundaries()
{
    for (const auto dpi : {96U, 120U, 192U})
    {
        const auto body =
            core::body_layout(1000, core::to_pixels(200, dpi), dpi, core::default_font_size());
        const auto large = core::status_bar_layout(1000, core::to_pixels(200, dpi), dpi);
        const auto space = large.items.front().left - core::to_pixels(28, dpi) - large.mode.right -
                           core::to_pixels(16, dpi);
        const auto threshold = 1000 - space + core::to_pixels(160, dpi);
        const auto exact = core::status_bar_layout(threshold, core::to_pixels(200, dpi), dpi);
        const auto short_by_one =
            core::status_bar_layout(threshold - 1, core::to_pixels(200, dpi), dpi);
        const auto shown = core::operation_guide_layout(GuideContext::other, body, exact, dpi);
        expect(std::holds_alternative<core::StatusGuideLayout>(shown),
               "exact 160 DIP reservation admits status guide at each DPI");
        verify_status_geometry(std::get<core::StatusGuideLayout>(shown), exact, dpi);
        expect(std::holds_alternative<std::monostate>(
                   core::operation_guide_layout(GuideContext::other, body, short_by_one, dpi)),
               "one physical pixel short of reservation hides the complete status guide");
    }
    const auto status = core::status_bar_layout(960, 265, 96);
    const auto exact = core::body_layout(960, 265, 96, core::default_font_size());
    const auto short_by_one = core::body_layout(960, 264, 96, core::default_font_size());
    expect(std::holds_alternative<core::BodyGuideLayout>(
               core::operation_guide_layout(GuideContext::untouched_untitled, exact, status, 96)),
           "exact vertical body margin admits body guide");
    expect(std::holds_alternative<core::StatusGuideLayout>(core::operation_guide_layout(
               GuideContext::untouched_untitled, short_by_one, status, 96)),
           "one pixel below body height boundary falls back to status alone");
}
} // namespace

void verify_guide_scope()
{
    verify_guide_text();
    verify_document_context();
    verify_sessions_and_ime();
    verify_recording_and_notice();
    verify_guide_setting_context();
    verify_geometry();
    verify_layout_boundaries();
}
} // namespace nenenib::tests
