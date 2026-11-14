#include "EditorControllerDetail.hpp"

#include "InputLinePrompt.hpp"
#include "LiteralPattern.hpp"
#include "TabTitle.hpp"
#include "UiTexts.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <utility>
#include <variant>

namespace nenenib::application::detail
{
[[nodiscard]] core::ScrollExtent scroll_extent_for(core::EditMode mode) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return core::ScrollExtent::filled_viewport;
    case core::EditMode::vim:
        return core::ScrollExtent::last_line;
    }
    std::unreachable();
}

// LF だけの本文を文書の改行に直す（ARC-009。改行の形を決めるのは controller の 1 か所）。
// Vim のレジスタは「行の列」なので LF で持ち、本文に入る瞬間にだけ文書の形になる（ADR 0015）。
[[nodiscard]] std::string with_document_newlines(std::string_view utf8, std::string_view newline)
{
    std::string result;
    result.reserve(utf8.size());
    for (const char byte : utf8)
    {
        if (byte == '\n')
        {
            result.append(newline);
            continue;
        }
        result.push_back(byte);
    }
    return result;
}
// incsearch が入力中のパターンを見せる状態か（ADR 0041 の決定 3・4）。検索の入力行（Vim の `/` `?`
// と通常モードの「検索: 」・ADR 0174 の決定 5）が開いていて、入力が空でなく、incsearch が on の
// ときだけ。「行: 」は検索ではないので見せない。あふれた行（確定で必ず断られる・#1568）も
// 見せない: 照合の費用は pattern の長さに比例して増えるので、働きの無い照合を打鍵ごとに走らせない
// （preview は消えて起点に戻り、Ctrl-G / Ctrl-T と入力中の塗りもここを読む）。
[[nodiscard]] const core::SearchLine *previewed_line(const EditorState &state)
{
    const auto &input = state.command_input();
    if (!state.vim().incsearch || !input.has_value())
    {
        return nullptr;
    }
    const auto *line = std::get_if<core::SearchLine>(&input.value());
    if (line == nullptr || line->text().empty() || line->overflowed() ||
        !searched_text(*line).has_value())
    {
        return nullptr;
    }
    return line;
}

std::optional<std::string> searched_text(const core::SearchLine &line)
{
    switch (line.prompt())
    {
    case core::InputLinePrompt::search_forward:
    case core::InputLinePrompt::search_backward:
        return std::string(line.text());
    // 通常モードの語は magic を解釈せず、逃がしてから同じ照合器へ（ADR 0174 の決定 4）。
    case core::InputLinePrompt::find:
        return core::literal_pattern(line.text(), line.direction());
    // 置換後の語は照合器へ渡さない（ADR 0174 の決定 7）。
    case core::InputLinePrompt::replace_with:
    case core::InputLinePrompt::go_to_line:
    case core::InputLinePrompt::ex:
    case core::InputLinePrompt::palette:
    case core::InputLinePrompt::substitute_confirm:
        return std::nullopt;
    }
    std::unreachable();
}

[[nodiscard]] std::optional<core::VimPattern> typed_pattern(const core::SearchLine &line,
                                                            core::VimPatternCase letter_case)
{
    const auto text = searched_text(line);
    if (!text.has_value())
    {
        return std::nullopt;
    }
    auto parsed = core::VimPattern::parse(text.value(), line.direction(), letter_case);
    if (!parsed)
    {
        return std::nullopt;
    }
    return std::move(parsed).value();
}
// 脇のタブの表示値。読み込み済みかどうかを区別せずに並べる（ADR 0059 の決定 4）。
[[nodiscard]] const DocumentView &parked_view(const ParkedTab &tab)
{
    return std::visit([](const auto &value) -> const DocumentView & { return value->view; }, tab);
}

[[nodiscard]] std::vector<DocumentView> tab_views(const EditorState &state,
                                                  const DocumentView &active)
{
    std::vector<DocumentView> views;
    views.reserve(state.tab_count());
    for (const auto &tab : state.parked())
    {
        if (views.size() == state.active_tab())
        {
            views.push_back(active);
        }
        views.push_back(parked_view(tab));
        // 脇の無題の題名は置いた時点の言語なので、今の表示言語で作り直す（#1019・経路のある題名は
        // 言語を問わない）。
        auto &parked = views.back();
        if (!parked.path.has_value())
        {
            parked.title =
                core::tab_title_for(std::nullopt, parked.save_state, state.display_language());
        }
    }
    if (views.size() == state.active_tab())
    {
        views.push_back(active);
    }
    // プレビューのタブの印は窓に 1 つで、束には持たない（ADR 0179 の決定 1・10）。
    const auto transient = state.transient_tab();
    if (transient.has_value())
    {
        views.at(transient.value()).transient = true;
    }
    return views;
}

// 脇のタブのパス。同じファイルの検索は、まだ読んでいない文書にもパスで当たる（ADR 0059 の決定 4）。
[[nodiscard]] const std::optional<core::FilePath> &
parked_path(const std::shared_ptr<const DocumentState> &tab)
{
    return tab->document.path;
}

[[nodiscard]] std::optional<core::FilePath>
parked_path(const std::shared_ptr<const UnloadedDocument> &tab)
{
    return tab->path;
}

// 覚えていた行を本文の範囲の中へ寄せる（行が無ければ最後の行・ADR 0059 の決定 5）。
[[nodiscard]] core::LineNumber line_within(const core::TextBuffer &text, core::LineNumber line)
{
    return core::LineNumber{std::clamp<std::size_t>(line.value, 1, text.line_count())};
}

// 名前を 1 つ載せる知らせの 1 行（ADR 0059 の「知らせの文言」）。others は同じ意図の中でほかに
// 読めなかった数。名前は 1 行に収まるよう末尾をコードポイントの境目で落とす（題名と同じ「…」）。
[[nodiscard]] core::DisplayText named_notice(core::UiMessage message,
                                             core::DisplayLanguage language, std::string name,
                                             std::size_t others)
{
    const std::string suffix = others == 0
                                   ? std::string{}
                                   : core::ui_text_filled(core::UiMessage::notice_others, language,
                                                          {std::to_string(others)});
    return named_line(message, language, std::move(name), suffix);
}

// 文の置き場 {0} の前を prefix、後ろを suffix の頭にして、上の形で名前を切る（#1019）。日本語の
// 「開けませんでした: {0}」は今までの prefix と同じ結果になる。
[[nodiscard]] core::DisplayText named_line(core::UiMessage message, core::DisplayLanguage language,
                                           std::string name, std::string_view suffix)
{
    const auto [before, after] = core::first_slot_split(core::ui_text(message, language));
    return named_line(before, std::move(name), std::string(after) + std::string(suffix));
}

[[nodiscard]] core::DisplayText named_line(std::string_view prefix, std::string name,
                                           std::string_view suffix)
{
    const std::size_t room = core::DisplayText::maximum_bytes - prefix.size() - suffix.size();
    if (name.size() > room)
    {
        core::Offset cut{room - clipped_mark.size()};
        while (!core::is_boundary(name, cut))
        {
            cut = core::previous_code_point(name, cut);
        }
        name.resize(cut.value);
        name += clipped_mark;
    }
    // 材料は検証済みの経路・題名と固定の文字だけなので parse は必ず成功する（不変条件・ARC-010）。
    return core::DisplayText::parse(std::string(prefix) + name + std::string(suffix)).value();
}

[[nodiscard]] bool measured(CommandWindow window) noexcept
{
    return std::get<0>(window) > 0 && std::get<1>(window) > 0;
}

// 状態の変換の表示値。本文と入力行のどちらに載せるかは呼び手が決める（ADR 0061 の決定 4）。
[[nodiscard]] std::optional<CompositionView>
composition_view_of(const std::optional<core::Composition> &composition)
{
    if (!composition.has_value())
    {
        return std::nullopt;
    }
    return CompositionView{composition.value().utf8,
                           core::composition_underlines(composition.value()),
                           composition.value().cursor};
}
bool untouched_untitled(const EditorState &state) noexcept
{
    return !state.document().path.has_value() && state.text().size_bytes() == 0 &&
           state.history().size() == 0;
}

// 合図（ADR 0062 の決定 7・ADR 0137 の決定 1・ADR 0155 の決定 8・ADR 0160 の決定 6・ADR 0176 の
// 決定 4）。「合図か」を決めるのはここの 1 か所（ADR 0062 の追補・#1296）。題名のダブルクリックの
// PinTab は使う人の操作なので合図ではない（ADR 0179 の決定 4-4）。
bool is_signal(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<WorkCompleted>(intent) ||
           std::holds_alternative<AutosaveTick>(intent) ||
           std::holds_alternative<PreviewTick>(intent) ||
           std::holds_alternative<LanguageServerTick>(intent) ||
           std::holds_alternative<MapTimeout>(intent) ||
           std::holds_alternative<FirstFramePresented>(intent);
}

// 使う人の操作ではない意図。合図（is_signal）と、窓の寸法・外観・帯とエクスプローラーの
// hover。keeps_tab_walk と begin_intent と `map`
// の待ちを保つ側と全面プレビューの振り分けが共用する。
bool not_user_operation(const EditorIntent &intent) noexcept
{
    return is_signal(intent) || std::holds_alternative<VisibleLines>(intent) ||
           std::holds_alternative<TitleBarWidth>(intent) ||
           std::holds_alternative<RefreshAppearance>(intent) ||
           std::holds_alternative<PointTitleBar>(intent) ||
           std::holds_alternative<HoverExplorer>(intent);
}

} // namespace nenenib::application::detail
