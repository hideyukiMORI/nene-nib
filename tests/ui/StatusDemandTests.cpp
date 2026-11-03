// ステータスの文字組みの保持の枠（ADR 0079 の決定 11 と追補・ADR 0154 の決定 6・#776）。
// renderer が 1 frame で使う枠の数を status_layout_demand で数え、保持の配列の長さ
// status_layout_capacity と照らす。案内が出る frame で 11（診断の件数があれば 12・ADR 0161 の
// 決定 4）・録画中 8（件数があれば 9）・言語を畳んだ狭い窓で案内が出るとしたら 10（最小の 360 DIP
// では案内は消えるので 6）。
#include "WindowChecks.hpp"

#include "BodyLayout.hpp"
#include "DevicePixels.hpp"
#include "DiagnosticCounts.hpp"
#include "DiagnosticsCount.hpp"
#include "EditorSettings.hpp"
#include "GuideContext.hpp"
#include "GuideRequest.hpp"
#include "OperationGuideLayout.hpp"
#include "StatusBarLayout.hpp"
#include "StatusGuideLayout.hpp"
#include "StatusLayoutDemand.hpp"
#include "StatusLeftState.hpp"

#include <cstddef>
#include <cstdint>
#include <variant>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace win32 = nenenib::ui::win32;
using win32::StatusLeftState;

constexpr core::DiagnosticCounts no_counts{0, 0, core::DiagnosticsCount::complete};
constexpr core::DiagnosticCounts counted{2, 1, core::DiagnosticsCount::truncated};

[[nodiscard]] core::StatusBarLayout status_at(std::int32_t width_dips, std::uint32_t dpi,
                                              const core::DiagnosticCounts &counts)
{
    return core::status_bar_layout(core::to_pixels(width_dips, dpi), core::to_pixels(450, dpi), dpi,
                                   counts);
}

[[nodiscard]] std::size_t demand_in(const core::StatusBarLayout &status, std::uint32_t dpi,
                                    StatusLeftState left, core::GuideContext context)
{
    const auto body = core::body_layout(status.band.right, status.band.bottom, dpi,
                                        core::default_editor_settings().font_size);
    const auto guide =
        core::operation_guide_layout(core::GuideRequest{context, 0}, body, status, dpi);
    return win32::status_layout_demand(left, status, guide);
}

[[nodiscard]] std::size_t demand_at(std::int32_t width_dips, std::uint32_t dpi,
                                    StatusLeftState left, core::GuideContext context)
{
    return demand_in(status_at(width_dips, dpi, no_counts), dpi, left, context);
}

[[nodiscard]] std::size_t counted_demand_at(std::int32_t width_dips, std::uint32_t dpi,
                                            StatusLeftState left, core::GuideContext context)
{
    return demand_in(status_at(width_dips, dpi, counted), dpi, left, context);
}
} // namespace

void verify_status_demand()
{
    for (const std::uint32_t dpi : {96U, 120U, 144U})
    {
        const auto guided = demand_at(900, dpi, StatusLeftState::mode, core::GuideContext::other);
        expect(guided == 11, "a 900 DIP frame with the status guide uses 11 retained layouts");
        const auto most =
            counted_demand_at(900, dpi, StatusLeftState::mode, core::GuideContext::other);
        expect(most == 12 && most == win32::status_layout_capacity,
               "the diagnostic count adds the 12th retained layout (ADR 0161)");
        expect(demand_at(900, dpi, StatusLeftState::recording, core::GuideContext::hidden) == 8 &&
                   counted_demand_at(900, dpi, StatusLeftState::recording,
                                     core::GuideContext::hidden) == 9,
               "recording hides the guide and uses 8, or 9 with the count");
        expect(demand_at(900, dpi, StatusLeftState::message, core::GuideContext::hidden) == 5 &&
                   demand_at(900, dpi, StatusLeftState::command_input,
                             core::GuideContext::hidden) == 4,
               "the message uses 1 and the : input none besides the four items");
        expect(demand_at(360, dpi, StatusLeftState::mode, core::GuideContext::other) == 6,
               "the minimum window folds the language and drops the guide");
        const auto narrow =
            core::status_bar_layout(core::to_pixels(360, dpi), core::to_pixels(450, dpi), dpi);
        expect(win32::status_layout_demand(StatusLeftState::mode, narrow,
                                           core::OperationGuideLayout{core::StatusGuideLayout{}}) ==
                   10,
               "a folded status bar with the guide would use 10");
    }
}
} // namespace nenenib::tests::ui
