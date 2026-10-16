// 本文を描いた最初の frame の節目 body_presented（ADR 0138 の決定 2・#546）の契約。窓は
// present が返った frame ごとに PresentedMilestones へ渡すだけで、節目を打つのはこの 1 か所。
// 替え玉の TimingPort に打たれた順を数える（時計は読まない）。
#include "PresentedMilestones.hpp"
#include "WindowChecks.hpp"

#include "Milestone.hpp"
#include "TimingPort.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
namespace win32 = nenenib::ui::win32;

class RecordedMilestones final : public app::TimingPort
{
  public:
    void mark(core::Milestone milestone) noexcept override
    {
        marks_.push_back(milestone);
    }

    [[nodiscard]] const std::vector<core::Milestone> &marks() const noexcept
    {
        return marks_;
    }

  private:
    std::vector<core::Milestone> marks_;
};

[[nodiscard]] std::size_t count_of(const std::vector<core::Milestone> &marks,
                                   core::Milestone milestone)
{
    return static_cast<std::size_t>(std::ranges::count(marks, milestone));
}

void present(win32::PresentedMilestones &milestones, RecordedMilestones &timing, bool pending)
{
    milestones.presented(pending, timing);
}

// 問いの無い文書: 最初の frame で frame_presented の直後に body_presented を 1 回。
void verify_body_on_the_first_frame()
{
    win32::PresentedMilestones milestones;
    RecordedMilestones timing;
    present(milestones, timing, false);
    present(milestones, timing, false);
    present(milestones, timing, false);
    const std::vector<core::Milestone> expected{
        core::Milestone::frame_presented, core::Milestone::body_presented,
        core::Milestone::frame_presented, core::Milestone::frame_presented};
    expect(timing.marks() == expected,
           "a document without the question marks body_presented once, in its first frame");
}

// 問いのある文書: 答えを待つ frame では打たず、答えの後の最初の frame で 1 回だけ。
void verify_body_after_the_answer()
{
    win32::PresentedMilestones milestones;
    RecordedMilestones timing;
    present(milestones, timing, true);
    present(milestones, timing, true);
    expect(count_of(timing.marks(), core::Milestone::body_presented) == 0,
           "frames waiting for the answer do not mark body_presented");
    present(milestones, timing, false);
    present(milestones, timing, false);
    const std::vector<core::Milestone> expected{
        core::Milestone::frame_presented, core::Milestone::frame_presented,
        core::Milestone::frame_presented, core::Milestone::body_presented,
        core::Milestone::frame_presented};
    expect(timing.marks() == expected,
           "a document with the question marks body_presented after the first frame_presented,"
           " in the first frame after the answer, once");
}
} // namespace

void verify_presented_milestones()
{
    verify_body_on_the_first_frame();
    verify_body_after_the_answer();
}
} // namespace nenenib::tests::ui
