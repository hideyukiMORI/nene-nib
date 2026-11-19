#pragma once

#include "LinkOpenFailure.hpp"

#include <expected>
#include <string>
#include <vector>

namespace nenenib::application
{
// プレビューのリンク（web と mail）を OS の既定の道（ブラウザ・メール）に渡す口（施主決定 D105・
// #1869 の段 L2・ADR 0004 の仕事の種類ごとの port・ARC-007: シェルに触れるのは
// src/adapters/win32 だけ）。uri は core::LinkDestination の text の綴りのまま。open は頼んで
// すぐ戻り（シェルの呼び出しは adapters がワーカーで行う）、終わった頼みの結果は WorkCompleted の
// 合図の後に collect が頼んだ順で全部返す（無ければ空）。document の宛先は渡さない（Nib の
// open_document の 1 本で開く）。UI スレッドだけが呼ぶ。controller が呼ぶのは札で開くときと、
// 頼んだ後の合図のときだけ（起動・開く・切り替え・保存・打鍵の道では呼ばない）。
class LinkOpenPort
{
  public:
    LinkOpenPort() = default;
    virtual ~LinkOpenPort() = default;
    LinkOpenPort(const LinkOpenPort &) = delete;
    LinkOpenPort(LinkOpenPort &&) = delete;
    LinkOpenPort &operator=(const LinkOpenPort &) = delete;
    LinkOpenPort &operator=(LinkOpenPort &&) = delete;

    virtual void open(std::string uri) = 0;
    [[nodiscard]] virtual std::vector<std::expected<void, LinkOpenFailure>> collect() = 0;
};
} // namespace nenenib::application
