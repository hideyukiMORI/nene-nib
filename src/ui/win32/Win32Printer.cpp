// CLSID_WICImagingFactory2 は wincodec.h の DEFINE_GUID で、定義は windowscodecs.lib にある。ui は
// その lib を結ばないので、この翻訳単位で INITGUID の形で定義を置く（selectany）。
#include <initguid.h>

#include <wincodec.h>

#include "Win32Printer.hpp"

#include "PrintComposition.hpp"
#include "PrintRendering.hpp"
#include "PrintSurface.hpp"
#include "Utf16.hpp"

#include <DocumentTarget.h>
#include <cstddef>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi.h>
#include <objbase.h>
#include <vector>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
namespace
{
constexpr auto rendering_failed = application::PrintFailure::rendering_failed;

// 印刷の device は WARP（GPU を起こさない・画面の device と別・device lost が無い）。描いた命令は
// print control が XPS へ写すので、ラスタライズの速さは要らない。
[[nodiscard]] PrintSurface make_surface()
{
    Microsoft::WRL::ComPtr<ID3D11Device> direct3d;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi;
    Microsoft::WRL::ComPtr<ID2D1Factory1> factory;
    PrintSurface surface;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                 D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                                 &direct3d, nullptr, nullptr)) ||
        FAILED(direct3d.As(&dxgi)) ||
        FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1),
                                 nullptr, &factory)) ||
        FAILED(factory->CreateDevice(dxgi.Get(), &surface.device)) ||
        FAILED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&surface.imaging))))
    {
        return {};
    }
    return surface;
}

// print ticket の XML のバイト列を stream に戻す（空なら null・プリンタの既定）。
[[nodiscard]] Microsoft::WRL::ComPtr<IStream> ticket_stream(const std::vector<std::byte> &ticket)
{
    Microsoft::WRL::ComPtr<IStream> stream;
    const LARGE_INTEGER origin{};
    if (ticket.empty() || FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) ||
        FAILED(stream->Write(ticket.data(), static_cast<ULONG>(ticket.size()), nullptr)) ||
        FAILED(stream->Seek(origin, STREAM_SEEK_SET, nullptr)))
    {
        return {};
    }
    return stream;
}

// 選んだプリンタへの job（名前は題名）の受け口。出力の stream は渡さず OS のスプールへ流す。
// 作った時点でスプールに job ができるので、これより後の失敗は render_print_job が Cancel する。
[[nodiscard]] Microsoft::WRL::ComPtr<IPrintDocumentPackageTarget>
spool_target(const application::PrintSettings &settings, const application::PrintJob &job)
{
    Microsoft::WRL::ComPtr<IPrintDocumentPackageTargetFactory> factory;
    Microsoft::WRL::ComPtr<IPrintDocumentPackageTarget> target;
    const auto printer = core::to_utf16(settings.printer);
    const auto name = core::to_utf16(job.title.text());
    const auto ticket = ticket_stream(settings.ticket);
    if (!printer || !name ||
        FAILED(CoCreateInstance(__uuidof(PrintDocumentPackageTargetFactory), nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
        FAILED(factory->CreateDocumentPackageTargetForPrintJob(
            printer.value().c_str(), name.value().c_str(), nullptr, ticket.Get(), &target)))
    {
        return {};
    }
    return target;
}
} // namespace

// device と紙面を先に作り、job の受け口はその後（受け口を作った後の失敗は Cancel の 1 か所）。
std::expected<application::PrintOutcome, application::PrintFailure>
Win32Printer::print(const application::PrintJob &job, const application::PrintSettings &settings)
{
    const auto surface = make_surface();
    if (!surface.device || !surface.imaging)
    {
        return std::unexpected(rendering_failed);
    }
    auto composition = PrintComposition::compose(job, settings.paper);
    if (!composition)
    {
        return std::unexpected(composition.error());
    }
    const auto target = spool_target(settings, job);
    if (!target)
    {
        return std::unexpected(rendering_failed);
    }
    const auto printed =
        render_print_job(surface, *target.Get(), composition.value(), settings.ranges);
    if (!printed)
    {
        return std::unexpected(printed.error());
    }
    return application::PrintOutcome::printed;
}
} // namespace nenenib::ui::win32
