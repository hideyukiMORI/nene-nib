#include "PrintRendering.hpp"

#include "PrintComposition.hpp"
#include "PrintPageDrawing.hpp"

#include <algorithm>
#include <cstddef>
#include <d2d1_1.h>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
namespace
{
constexpr auto rendering_failed = application::PrintFailure::rendering_failed;

// 1 始まりのページ number を選んだ範囲が含むか。範囲が無ければ全ページ。
[[nodiscard]] bool wanted(const PrintSetup &setup, std::size_t number) noexcept
{
    return setup.ranges.empty() ||
           std::ranges::any_of(setup.ranges, [number](const PrintRange &range)
                               { return range.first <= number && number <= range.last; });
}

[[nodiscard]] std::expected<void, application::PrintFailure> add_page(ID2D1DeviceContext &context,
                                                                      ID2D1PrintControl &control,
                                                                      PrintComposition &composition,
                                                                      std::size_t index)
{
    Microsoft::WRL::ComPtr<ID2D1CommandList> commands;
    if (FAILED(context.CreateCommandList(&commands)))
    {
        return std::unexpected(rendering_failed);
    }
    context.SetTarget(commands.Get());
    context.BeginDraw();
    const auto drawn = draw_print_page(context, composition.page(index));
    const auto ended = context.EndDraw();
    context.SetTarget(nullptr);
    if (!drawn || FAILED(ended) || FAILED(commands->Close()) ||
        FAILED(control.AddPage(commands.Get(), composition.paper_size(), nullptr)))
    {
        return std::unexpected(rendering_failed);
    }
    return {};
}

[[nodiscard]] std::expected<void, application::PrintFailure>
add_pages(const PrintSurface &surface, ID2D1PrintControl &control, PrintComposition &composition,
          const PrintSetup &setup)
{
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> context;
    if (FAILED(surface.device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context)))
    {
        return std::unexpected(rendering_failed);
    }
    for (std::size_t index = 0; index < composition.page_count(); ++index)
    {
        if (!wanted(setup, index + 1))
        {
            continue;
        }
        const auto added = add_page(*context.Get(), control, composition, index);
        if (!added)
        {
            return added;
        }
    }
    return {};
}
} // namespace

std::expected<void, application::PrintFailure> render_print_job(const PrintSurface &surface,
                                                                IPrintDocumentPackageTarget &target,
                                                                const application::PrintJob &job,
                                                                const PrintSetup &setup)
{
    auto composition = PrintComposition::compose(job, setup.paper);
    if (!composition)
    {
        return std::unexpected(composition.error());
    }
    Microsoft::WRL::ComPtr<ID2D1PrintControl> control;
    if (!surface.device || !surface.imaging ||
        FAILED(
            surface.device->CreatePrintControl(surface.imaging.Get(), &target, nullptr, &control)))
    {
        return std::unexpected(rendering_failed);
    }
    const auto added = add_pages(surface, *control.Get(), composition.value(), setup);
    if (!added || FAILED(control->Close()))
    {
        static_cast<void>(target.Cancel());
        return std::unexpected(rendering_failed);
    }
    return {};
}
} // namespace nenenib::ui::win32
