#pragma once

#include "FoundLanguageServer.hpp"
#include "LanguageServerProbe.hpp"
#include "LanguageServerSearchPort.hpp"
#include "ServerSearchPlaces.hpp"
#include "ServerSearchShelf.hpp"
#include "Win32Worker.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace nenenib::adapters::win32
{
// PC の中の言語サーバーを裏で探す（LSP 段 S4・#1945）。探しはワーカーを借りて find_language_servers
// を 1 回呼ぶ（置き場は places がその仕事の中で作る・製品は system_search_places・試験は一時の
// フォルダ）。結果は棚にたまり、「届いた」の合図は bind で後から受け取る。HWND も窓メッセージの
// 番号も知らない。
class Win32LanguageServerSearch final : public application::LanguageServerSearchPort
{
  public:
    Win32LanguageServerSearch(Win32Worker &worker, std::function<ServerSearchPlaces()> places);
    ~Win32LanguageServerSearch() override;
    Win32LanguageServerSearch(const Win32LanguageServerSearch &) = delete;
    Win32LanguageServerSearch(Win32LanguageServerSearch &&) = delete;
    Win32LanguageServerSearch &operator=(const Win32LanguageServerSearch &) = delete;
    Win32LanguageServerSearch &operator=(Win32LanguageServerSearch &&) = delete;

    // 合図はワーカーのスレッドから、たまりが空から値ありになったときだけ呼ばれる。
    void bind(std::function<void()> arrived);
    void search(std::vector<application::LanguageServerProbe> probes) override;
    [[nodiscard]] std::optional<std::vector<std::optional<application::FoundLanguageServer>>>
    collect() override;

  private:
    Win32Worker &worker_;
    std::shared_ptr<std::function<ServerSearchPlaces()>> places_;
    std::shared_ptr<ServerSearchShelf> shelf_;
};
} // namespace nenenib::adapters::win32
