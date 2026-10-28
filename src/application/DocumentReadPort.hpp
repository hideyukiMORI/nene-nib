#pragma once

#include "DocumentReadOutcome.hpp"
#include "DocumentReadRequest.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace nenenib::application
{
// 文書を開く道の読みの口（ADR 0159 の決定 1・3・5）。FolderPort（ADR 0062）と同じ形で、スレッドは
// adapters に閉じる。request は頼んですぐ戻る。呼んだスレッドで揃えられた（小さなファイル）なら
// その結果を返し、裏へ回したなら値なし。cancel はその券の仕事をやめさせる（結果は出ない）。
// collect は揃った結果と進み具合を全部受け取る（無ければ空）。どれも UI スレッドだけが呼ぶ。
class DocumentReadPort
{
  public:
    DocumentReadPort() = default;
    virtual ~DocumentReadPort() = default;
    DocumentReadPort(const DocumentReadPort &) = delete;
    DocumentReadPort(DocumentReadPort &&) = delete;
    DocumentReadPort &operator=(const DocumentReadPort &) = delete;
    DocumentReadPort &operator=(DocumentReadPort &&) = delete;

    [[nodiscard]] virtual std::optional<DocumentReadOutcome>
    request(DocumentReadRequest request) = 0;
    virtual void cancel(std::uint64_t ticket) = 0;
    [[nodiscard]] virtual std::vector<DocumentReadOutcome> collect() = 0;
};
} // namespace nenenib::application
