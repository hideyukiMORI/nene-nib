#pragma once

#include "MarkdownOutcome.hpp"
#include "MarkdownRequest.hpp"

#include <vector>

namespace nenenib::application
{
// Markdown の本文を裏で解析する口（ADR 0155 の決定 5・ADR 0004 の仕事の種類ごとの port）。
// スレッドは adapters に閉じる。形は FolderPort と同じで、合図を結ぶ bind は adapter が持つ。
// parse は頼んですぐ戻る。前の要求はやめてよく、結果は券で見分ける。
// collect は届いた結果を全部受け取る（無ければ空）。UI スレッドだけが呼ぶ。
class MarkdownPort
{
  public:
    MarkdownPort() = default;
    virtual ~MarkdownPort() = default;
    MarkdownPort(const MarkdownPort &) = delete;
    MarkdownPort(MarkdownPort &&) = delete;
    MarkdownPort &operator=(const MarkdownPort &) = delete;
    MarkdownPort &operator=(MarkdownPort &&) = delete;

    virtual void parse(MarkdownRequest request) = 0;
    [[nodiscard]] virtual std::vector<MarkdownOutcome> collect() = 0;
};
} // namespace nenenib::application
