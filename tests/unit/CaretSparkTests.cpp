// scope `--caret-motion` の打ったときの粒（ADR 0192 の追補・#1979）。粒の純関数（決定的・寿命・扇の
// 向き・大きさ）、覚えの進め方（最初の frame・打鍵で束・上限・寿命の後に止まる・止める frame）、打
// 鍵の通し番号（オフで進まない・打つと進む・貼り付け・再生・`.`・undo・移動・日時では進まない）と
// `:set caretsparks`・設定の画面の行。時計・乱数・ファイルを読まない。
#include "Appearance.hpp"
#include "CaretMotion.hpp"
#include "CaretMotionClock.hpp"
#include "CaretMotionTick.hpp"
#include "CaretPoint.hpp"
#include "CaretSparkTrack.hpp"
#include "CaretSparks.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFeature.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExResult.hpp"
#include "FeatureCosts.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertDateTime.hpp"
#include "InsertText.hpp"
#include "MoveCaret.hpp"
#include "PasteCommand.hpp"
#include "Scopes.hpp"
#include "SelectionAnchoring.hpp"
#include "SettingsRows.hpp"
#include "SettingsSubject.hpp"
#include "TestSupport.hpp"
#include "VimKeyPress.hpp"
#include "VimTestSupport.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
constexpr core::CaretPoint origin{100.0F, 50.0F};

void verify_spark_shapes()
{
    const auto first = core::caret_spark_at(5, 0, 100);
    const auto again = core::caret_spark_at(5, 0, 100);
    expect(first.has_value() && again.has_value() && first.value().x == again.value().x &&
               first.value().y == again.value().y && first.value().size == again.value().size &&
               first.value().tinted == again.value().tinted,
           "the same keystroke number and age give the same spark");
    bool differs = false;
    for (std::size_t index = 1; index < core::caret_spark_count; ++index)
    {
        const auto other = core::caret_spark_at(5, index, 100);
        differs = differs ||
                  (other.has_value() && other.value().x != first.value_or(core::CaretSpark{}).x);
    }
    expect(differs, "the sparks of one keystroke scatter in different directions");
    const auto born = core::caret_spark_at(9, 3, 0);
    expect(born.has_value() && born.value().x == 0.0F && born.value().y == 0.0F &&
               born.value().opacity == 1.0F,
           "a spark starts at the typed place fully opaque");
    expect(!core::caret_spark_at(9, 3, core::caret_spark_life).has_value() &&
               core::caret_spark_at(9, 3, core::caret_spark_life - 1).has_value(),
           "a spark lives 420 ms");
}

// 40 打鍵ぶんの粒が扇の中・大きさの範囲・薄れの途中にあり、2 つ目の色が混じる。
void verify_spark_fan()
{
    bool inside = true;
    std::size_t tinted = 0;
    for (std::uint32_t serial = 1; serial <= 40; ++serial)
    {
        for (std::size_t index = 0; index < core::caret_spark_count; ++index)
        {
            const auto spark = core::caret_spark_at(serial, index, 50).value_or(core::CaretSpark{});
            // 50 ms では毎秒 130 DIP の粒でも 6.5 DIP しか進まず、どれも上へ出る（扇は真上から±1.1
            // rad）。
            inside = inside && spark.size >= 2.0F && spark.size <= 4.5F && spark.x >= -6.5F &&
                     spark.x <= 6.5F && spark.y < 0.0F && spark.opacity < 1.0F &&
                     spark.opacity > 0.0F;
            tinted += spark.tinted ? 1U : 0U;
        }
    }
    expect(inside, "sparks fly up inside the fan, 2 to 4.5 DIP wide, fading");
    expect(tinted > 40U && tinted < 180U, "some sparks take the second colour");
}

[[nodiscard]] core::CaretSparkTrack followed(const core::CaretSparkTrack &track,
                                             std::uint32_t serial, core::CaretMotionClock progress,
                                             const std::optional<core::CaretPoint> &at = origin)
{
    return core::caret_sparks_followed(track, serial, at, progress);
}

void verify_spark_restart(const core::CaretSparkTrack &done, const core::CaretSparkTrack &first);

void verify_spark_track()
{
    const auto first = followed({}, 4, {});
    expect(!first.alive && first.count == 0 && first.serial == std::optional<std::uint32_t>{4},
           "the first frame only remembers the keystroke number");
    const auto still = followed(first, 4, {});
    expect(!still.alive, "an unchanged keystroke number scatters nothing");
    const auto typed = followed(first, 5, {});
    expect(typed.alive && typed.count == 1 && typed.generation == 1 && typed.now == 0 &&
               typed.bursts.at(0).serial == 5 && typed.bursts.at(0).origin == origin,
           "a new keystroke scatters a burst at the caret and starts spark clock 1");
    const auto hidden = followed(first, 5, {}, std::nullopt);
    expect(!hidden.alive && hidden.serial == std::optional<std::uint32_t>{5},
           "a keystroke whose caret is not drawn scatters nothing");
    const auto stale = followed(typed, 5, {0, 300});
    expect(stale.alive && stale.now == 0, "the elapsed time of another spark clock is not read");
    const auto ticked = followed(typed, 5, {1, 200});
    const auto second = followed(ticked, 6, {1, 200});
    expect(second.count == 2 && second.bursts.at(1).born == 200,
           "a keystroke while sparks live adds a burst born at the clock's time");
    const auto older = followed(second, 6, {1, core::caret_spark_life});
    expect(older.count == 1 && older.bursts.at(0).serial == 6, "the expired burst is dropped");
    const auto done = followed(older, 6, {1, 200 + core::caret_spark_life});
    expect(!done.alive && done.count == 0, "after the life no spark is left and the clock stops");
    verify_spark_restart(done, first);
}

// 寿命の後の次の打鍵・速い連打の上限・止める frame。
void verify_spark_restart(const core::CaretSparkTrack &done, const core::CaretSparkTrack &first)
{
    const auto restarted = followed(done, 7, {1, 900});
    expect(restarted.alive && restarted.generation == 2 && restarted.now == 0,
           "the next keystroke starts the next spark clock from 0");
    auto burst = followed(first, 5, {});
    for (std::uint32_t serial = 6; serial < 20; ++serial)
    {
        burst = followed(burst, serial, {1, 10});
    }
    expect(burst.count == core::caret_spark_bursts && burst.bursts.at(0).serial == 12 &&
               burst.bursts.at(core::caret_spark_bursts - 1).serial == 19,
           "fast typing keeps only the last 8 bursts");
    const auto paused = core::caret_sparks_paused(burst, 25);
    expect(!paused.alive && paused.count == 0 && paused.serial == std::optional<std::uint32_t>{25},
           "a paused frame drops the sparks and remembers the number");
    expect(!followed(paused, 25, {}).alive, "keystrokes while paused never scatter later");
}

[[nodiscard]] core::EditorSettings sparks_on()
{
    auto settings = core::default_editor_settings();
    settings.caret_sparks = true;
    return settings;
}

void verify_typed_serial_ordinary()
{
    Editing off;
    applied(off.controller(), application::InsertText{"a"});
    expect(off.controller().frame().typed == 0, "with the sparks off typing counts nothing");
    Editing editing(std::optional<core::EditorSettings>{sparks_on()});
    auto &controller = editing.controller();
    applied(controller, application::InsertText{"a"});
    applied(controller, application::InsertText{"b"});
    expect(controller.frame().typed == 2, "each typed character advances the keystroke number");
    editing.clipboard().hold(std::string("pasted"));
    applied(controller, application::PasteCommand{});
    applied(controller, application::MoveCaret{core::CaretMotion::previous_character,
                                               core::SelectionAnchoring::collapse});
    applied(controller, application::HistoryAction{core::HistoryDirection::undo});
    applied(controller, application::HistoryAction{core::HistoryDirection::redo});
    applied(controller, application::InsertDateTime{});
    applied(controller, application::CaretMotionTick{{1, 10}, {1, 10}});
    expect(controller.frame().typed == 2,
           "paste, caret moves, undo, redo, the date and the tick do not count");
}

void press(application::EditorController &controller, std::string_view keys)
{
    for (const auto &key : vim_keys_of(keys))
    {
        applied(controller, application::VimKeyPress{key});
    }
}

void verify_typed_serial_vim()
{
    Editing editing(std::optional<core::EditorSettings>{sparks_on()});
    open_vim_document(editing, "abc\n");
    auto &controller = editing.controller();
    press(controller, "ixy");
    expect(controller.frame().typed == 2, "characters typed in INSERT count, the i does not");
    press(controller, "<Esc>.");
    expect(controller.frame().typed == 2 && whole_vim_body(controller) == "xxyyabc\n",
           "escape and the dot repeat do not count");
    vim_replay(controller, "iz<Esc>");
    expect(controller.frame().typed == 2, "replayed keys do not count");
    press(controller, "u");
    expect(controller.frame().typed == 2, "undo does not count");
    Editing off;
    open_vim_document(off, "abc\n");
    press(off.controller(), "ixy");
    expect(off.controller().frame().typed == 0, "with the sparks off Vim typing counts nothing");
}

// `:set` が置いた設定（問い・失敗は値なし）。
[[nodiscard]] std::optional<core::EditorSettings> set_by(std::string_view text,
                                                         const core::EditorSettings &settings)
{
    const auto result = core::evaluate_ex(text, settings, core::Appearance::dark);
    if (!result.has_value())
    {
        return std::nullopt;
    }
    return result.value().settings;
}

void verify_spark_settings()
{
    const auto settings = core::default_editor_settings();
    expect(!settings.caret_sparks, "the sparks are off by default");
    const auto on = set_by("set caretsparks", settings);
    expect(on.has_value() && on.value_or(settings).caret_sparks &&
               !on.value_or(settings).caret_motion,
           ":set caretsparks turns the sparks on without the motion");
    const auto off = set_by("set nocaretsparks", on.value_or(settings));
    expect(off.has_value() && !off.value_or(settings).caret_sparks,
           ":set nocaretsparks turns them off");
    const auto asked = core::evaluate_ex("set caretsparks?", settings, core::Appearance::dark);
    expect(asked.has_value() && !asked.value().settings.has_value() &&
               asked.value().message.text() == "caretsparks=off",
           ":set caretsparks? answers without changing");
    expect(core::feature_switchable(core::EditorFeature::caret_sparks) &&
               core::feature_enabled(
                   core::feature_set(settings, core::EditorFeature::caret_sparks, true),
                   core::EditorFeature::caret_sparks),
           "the settings switch reads and writes the own key");
    expect(std::ranges::any_of(core::settings_rows,
                               [](const core::SettingsRow &row)
                               {
                                   return row.section == core::SettingsSection::editing &&
                                          row.subject == core::SettingsSubject{
                                                             core::EditorFeature::caret_sparks};
                               }),
           "the settings panel has the sparks row in the editing section");
}
} // namespace

void verify_caret_sparks_contracts()
{
    verify_spark_shapes();
    verify_spark_fan();
    verify_spark_track();
    verify_typed_serial_ordinary();
    verify_typed_serial_vim();
    verify_spark_settings();
}
} // namespace nenenib::tests
