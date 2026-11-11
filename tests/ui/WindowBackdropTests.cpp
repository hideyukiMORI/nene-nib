#include "ExitPlan.hpp"
#include "WindowBackdrop.hpp"
#include "WindowChecks.hpp"

namespace nenenib::tests::ui
{
// 描画を失って取消の無い箱を聞く間だけの窓の下地（#1499）。
// 掛けるのは取消の無い箱の扱いだけで、掛ける口は DWM の属性 DWMWA_SYSTEMBACKDROP_TYPE を試みて
// 結果を返す（成否は OS の版に依るので製品は見ない）。窓は作らない: 窓の無い呼び出しは DWM が
// 失敗で返し、止まらないことだけを見る。
void verify_question_backdrop()
{
    using nenenib::application::exit_plan;
    using nenenib::application::ExitCause;
    using nenenib::application::ExitPlan;
    using nenenib::ui::win32::needs_question_backdrop;
    expect(needs_question_backdrop(ExitPlan::confirm_each_tab_without_cancel),
           "the question without cancel after the rendering is lost shows a backdrop");
    expect(!needs_question_backdrop(ExitPlan::confirm_each_tab) &&
               !needs_question_backdrop(ExitPlan::block_session_end) &&
               !needs_question_backdrop(ExitPlan::leave_now),
           "a window that still draws, or closes at once, gets no backdrop");
    // end_window が通る道ごと: abandon（rendering_lost）で未保存があれば掛ける。recover()
    // が成功した 道は end_window に来ない。使う人の × と OS の終了と、未保存の無い abandon
    // では掛けない。
    expect(needs_question_backdrop(exit_plan(ExitCause::rendering_lost, 1)),
           "abandon with an unsaved tab asks for the backdrop");
    expect(!needs_question_backdrop(exit_plan(ExitCause::rendering_lost, 0)),
           "abandon with nothing to ask leaves at once without the backdrop");
    expect(!needs_question_backdrop(exit_plan(ExitCause::user_close, 1)) &&
               !needs_question_backdrop(exit_plan(ExitCause::session_end, 1)),
           "the close button and the OS shutdown get no backdrop");
    expect(nenenib::ui::win32::question_backdrop == DWMSBT_MAINWINDOW,
           "the backdrop under the question is the main window Mica");
    expect(FAILED(nenenib::ui::win32::show_question_backdrop(nullptr)),
           "asking for the backdrop without a window fails quietly and returns");
}
} // namespace nenenib::tests::ui
