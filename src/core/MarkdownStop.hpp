#pragma once

namespace nenenib::core
{
// parse を打ち切るかを問う型のある述語（ADR 0151 の決定 6）。core は時計も印も持たないので、
// 呼び出し側（adapters の worker）が自分の印を読む呼び出し可能な値を貸す。<functional> を core に
// 持ち込まず、関数ポインタと借りた文脈の対で持つ。貸した値は parse_markdown が返るまで生かす。
class MarkdownStop final
{
  public:
    // ask は const で呼べて bool を返す値。参照を借りるだけで写さない。
    template <class Ask> [[nodiscard]] static MarkdownStop asking(const Ask &ask) noexcept
    {
        return MarkdownStop(&ask, [](const void *context)
                            { return static_cast<bool>((*static_cast<const Ask *>(context))()); });
    }
    // 打ち切らない述語。
    [[nodiscard]] static MarkdownStop never() noexcept
    {
        return MarkdownStop(nullptr, [](const void *) { return false; });
    }
    [[nodiscard]] bool should_stop() const
    {
        return ask_(context_);
    }

  private:
    using Ask = bool (*)(const void *);
    MarkdownStop(const void *context, Ask ask) noexcept : context_(context), ask_(ask) {}
    const void *context_;
    Ask ask_;
};
} // namespace nenenib::core
