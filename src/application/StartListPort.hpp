#pragma once

#include "StartLists.hpp"

#include <optional>

namespace nenenib::application
{
// 起動画面の履歴とブックマークを裏で 1 回読む口（ADR 0172 の決定 3・ADR 0004 の仕事の種類ごとの
// port・形は FolderPort と同じで、合図を結ぶ bind は adapter が持つ）。読むのは Ctrl+P の面と同じ
// HistoryPort::read と BookmarkPort::read（読み口を 2 本にしない）で、スレッドは adapters に
// 閉じる。request は頼んですぐ戻る。collect は届いた結果を受け取る（まだなら値なし）。UI
// スレッドだけが呼ぶ。
class StartListPort
{
  public:
    StartListPort() = default;
    virtual ~StartListPort() = default;
    StartListPort(const StartListPort &) = delete;
    StartListPort(StartListPort &&) = delete;
    StartListPort &operator=(const StartListPort &) = delete;
    StartListPort &operator=(StartListPort &&) = delete;

    virtual void request() = 0;
    [[nodiscard]] virtual std::optional<StartLists> collect() = 0;
};
} // namespace nenenib::application
